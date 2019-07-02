// Job queue + Job pool
// ===========================================================================



#include "jobpool.h"

// cstd
#include <stdio.h>
#include <stdlib.h>
// system
#include <pthread.h>
#include <unistd.h>


struct jobpool {
    struct jobnode * free_pool;         // flock
    struct jobnode * * blocking_map;    // flock read, write ???
    struct jobnode * active_queue_front;    // qlock
    struct jobnode * active_queue_rear;     // qlock
    int free_count;     // flock
    int queue_count;    // qlock
    int size;           // immutable
    pthread_spinlock_t flock; // DEVNOTE: Using spinlock since I don't want context switch in case of wait
    pthread_spinlock_t qlock; // DEVNOTE: Using spinlock since I don't want context switch in case of wait
} _jobpool = {
    .free_pool = NULL, 
    .blocking_map = NULL, 
    .active_queue_front = NULL, 
    .active_queue_rear = NULL, 
    .free_count = 0, 
    .queue_count = 0, 
    .size = 0};


// TODO: counterpart destroy function
int jobpool_init(int size)
{
    struct jobnode * jobs = malloc(sizeof(struct jobnode) * size);
    if (NULL == jobs) { 
        perror("jobpool: malloc: allocating jobs");
        return -1;
    }
    
    _jobpool.blocking_map = malloc(sizeof(struct jobnode *) * size);
    if (NULL == jobs) {
        free(jobs);
        perror("jobpool: malloc: allocating map");
        return -1;
    }

    for (int i = 0; i < size - 1; i++) {
        jobs[i].nextfree = &jobs[i+1];
        jobs[i].state = JOB_UNINITED;
        _jobpool.blocking_map[i] = NULL;
    }
    jobs[size - 1].next = NULL;
    _jobpool.blocking_map[size - 1] = NULL;

    _jobpool.free_pool = jobs;
    _jobpool.free_count = size;
    _jobpool.size = size;

    int ret = -1;
    ret = pthread_spin_init(&_jobpool.flock, PTHREAD_PROCESS_PRIVATE);
    if (ret != 0) {
        free(jobs);
        free(_jobpool.blocking_map);
        perror("jobpool: pthread_spin_init: freepool");
        return -1;
    }

    ret = pthread_spin_init(&_jobpool.qlock, PTHREAD_PROCESS_PRIVATE);
    if (ret != 0) {
        free(jobs);
        free(_jobpool.blocking_map);
        pthread_spin_destroy(&_jobpool.flock); // TODO: even if spin destroy fails, can't do anything about it
        perror("jobpool: pthread_spin_init: queue");
        return -1;
    }

    return 0;
}


struct jobnode * jobpool_get(int sockfd) {
    if (sockfd > _jobpool.size) {
        return NULL;
    }

    return _jobpool.blocking_map[sockfd];
}



#if 0
// WARN: SINGLE-THREADED USE ONLY
struct jobnode * jobpool_blocked_get(int sockfd) {
    if (sockfd > _jobpool.size) { // TODO: Non thread safe access
        return NULL;
    }

    if (pthread_spin_lock(&_jobpool.block) != 0) goto EXIT;

    struct jobnode * temp = _jobpool.blocking_map[sockfd];

    if (pthread_spin_unlock(&_jobpool.block) != 0) goto EXIT;

    return temp;

EXIT:
    perror("jobpool: get - spin lock/unlock failed");
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
    return NULL;
}

struct jobnode * jobpool_blocked_delete(int sockfd) {
    if (sockfd > _jobpool.size) { // TODO: Non thread safe access
        fprintf(stderr, "jobpool: socket larger than map\n");
        exit(1); // TODO: serious unhandled case where sockfd exceeds MAX_CONNECTIONS
    }

    if (pthread_spin_lock(&_jobpool.block) != 0) goto EXIT;

    struct jobnode * temp = _jobpool.blocking_map[sockfd];
    _jobpool.blocking_map[sockfd] = NULL;

    if (pthread_spin_unlock(&_jobpool.block) != 0) goto EXIT;

    return temp;

EXIT:
    perror("jobpool: delete - spin lock/unlock failed");
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
    return NULL;
}

int jobpool_blocked_put(int sockfd, struct jobnode* job) {
    if (sockfd > _jobpool.size) { // TODO: Non thread safe access
        fprintf(stderr, "jobpool: socket larger than map\n");
        exit(1); // TODO: serious unhandled case where sockfd exceeds MAX_CONNECTIONS
    }

    if (pthread_spin_lock(&_jobpool.block) != 0) goto EXIT;

    _jobpool.blocking_map[sockfd] = job;

    if (pthread_spin_unlock(&_jobpool.block) != 0) goto EXIT;

    return 0;

EXIT:
    perror("jobpool: put - spin lock/unlock failed");
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
}
#endif


struct jobnode * jobpool_free_acquire(int sockfd)
{
    
    struct jobnode * temp = NULL;

    if (pthread_spin_lock(&_jobpool.flock) != 0) goto EXIT;

    if (_jobpool.free_count > 0) {
        temp = _jobpool.free_pool;
        _jobpool.free_pool = _jobpool.free_pool->nextfree;
        _jobpool.free_count -= 1;

        _jobpool.blocking_map[sockfd] = temp;
    } else {
        fprintf(stderr, "TODO: unhandler error of running out of free connections\n");
        exit(1);
    }

    if (pthread_spin_unlock(&_jobpool.flock) != 0) goto EXIT;

    // DEVNOTE: Any object which just came out of the freepool should not need to be synchronise.
    //          Synchronisation issues will only appear after first dequeueing.
    if (temp != NULL) {
        temp->sockfd = sockfd;
        temp->yielded = false;
        temp->next = NULL;
        temp->prev = NULL;
        temp->nextfree = NULL;
    }

    return temp;
    
EXIT:
    perror("jobpool: acquire - spin lock/unlock failed");
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
    return NULL;
}

void jobpool_free_release(int sockfd)
{
    
    if (pthread_spin_lock(&_jobpool.flock) != 0) goto EXIT;
    
    struct jobnode * released = _jobpool.blocking_map[sockfd];

    released->nextfree = _jobpool.free_pool;
    _jobpool.free_pool = released;
    _jobpool.free_count += 1;

    _jobpool.blocking_map[sockfd] = NULL;

    if (pthread_spin_unlock(&_jobpool.flock) != 0) goto EXIT;
    
    return;

EXIT:
    perror("jobpool: release - spin lock/unlock failed");
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
}

// enqueque new job
void jobq_active_enqueue(struct jobnode * job)
{

    job->next = NULL;

    if (pthread_spin_lock(&_jobpool.qlock) != 0) goto EXIT;

    if (_jobpool.queue_count > 0) {
        _jobpool.active_queue_rear->next = job;
        job->prev = _jobpool.active_queue_rear;
    } else {
        job->prev = NULL;
        _jobpool.active_queue_front = job;        
    }

    _jobpool.active_queue_rear = job;
    _jobpool.queue_count += 1;

    if (pthread_spin_unlock(&_jobpool.qlock) != 0) goto EXIT;

    return;

EXIT:
    perror("jobpool: enqueue - spin lock/unlock failed");
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
}

struct jobnode * jobq_active_dequeue(void)
{
    struct jobnode * temp = NULL;

    if (pthread_spin_lock(&_jobpool.qlock) != 0) goto EXIT;

    temp = _jobpool.active_queue_front;
    if (_jobpool.queue_count > 1) {
        _jobpool.active_queue_front = temp->next;
        _jobpool.active_queue_front->prev = NULL;
        _jobpool.queue_count -= 1;
        temp->next = NULL;
    } else if (_jobpool.queue_count == 1) {
        _jobpool.active_queue_front = NULL;
        _jobpool.active_queue_rear = NULL;
        _jobpool.queue_count -= 1;
        temp->next = NULL;
    }

    if (pthread_spin_unlock(&_jobpool.qlock) != 0) goto EXIT;

    return temp;
    
EXIT:
    perror("jobpool: dequeue - spin lock/unlock failed");
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
}




