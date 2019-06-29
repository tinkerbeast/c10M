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
    struct jobnode * free_pool;
    struct jobnode * * blocking_map;
    struct jobnode * active_queue;
    int free_count;
    int queue_count;
    int size;
    pthread_spinlock_t flock; // DEVNOTE: Using spinlock since I don't want context switch in case of wait
    pthread_spinlock_t qlock; // DEVNOTE: Using spinlock since I don't want context switch in case of wait
    pthread_spinlock_t block; // DEVNOTE: Using spinlock since I don't want context switch in case of wait
} _jobpool = {
    .free_pool = NULL, 
    .blocking_map = NULL, 
    .active_queue = NULL, 
    .free_count = 0, 
    .queue_count = 0, 
    .size = 0};


// TODO: counterpart destroy function
int jobpool_init(int size) {
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
        jobs[i].next = &jobs[i+1];
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

    // TODO: resource freeing for failure case
    return pthread_spin_init(&_jobpool.block, PTHREAD_PROCESS_PRIVATE);
}

// WARN: SINGLE-THREADED USE ONLY
struct jobnode * jobpool_blocked_get(int sockfd) {
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
    // TODO: error prints
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
    // TODO: error prints
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
}


struct jobnode * jobpool_free_acquire(void) {
    
    struct jobnode * temp = NULL;

    if (pthread_spin_lock(&_jobpool.flock) != 0) goto EXIT;

    if (_jobpool.free_count > 0) {
        temp = _jobpool.free_pool;
        _jobpool.free_pool = _jobpool.free_pool->next;
        _jobpool.free_count -= 1;
    }

    if (pthread_spin_unlock(&_jobpool.flock) != 0) goto EXIT;

    if (temp != NULL) {
        temp->sockfd = -1;
        temp->yielded = false;
        temp->closed = false;
        temp->next = NULL;
        temp->prev = NULL;
    }

    return temp;
    
EXIT:
    // TODO: error prints
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
    return NULL;
}

void jobpool_free_release(struct jobnode* released) {
    
    if (pthread_spin_lock(&_jobpool.flock) != 0) goto EXIT;

    released->next = _jobpool.free_pool;
    _jobpool.free_pool = released;
    _jobpool.free_count += 1;

    if (pthread_spin_unlock(&_jobpool.flock) != 0) goto EXIT;
    
EXIT:
    // TODO: error prints
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
}

// enqueque new job
void jobpool_active_enqueue(struct jobnode * job) {

    if (pthread_spin_lock(&_jobpool.qlock) != 0) goto EXIT;

    if (_jobpool.queue_count > 0) {
        job->next = _jobpool.active_queue;
        job->prev = _jobpool.active_queue->prev;
        _jobpool.active_queue->prev->next = job;
        _jobpool.active_queue->prev = job;
    } else {
        job->next = job;
        job->prev = job;
        _jobpool.active_queue = job;
    }

    _jobpool.queue_count += 1;

    if (pthread_spin_unlock(&_jobpool.qlock) != 0) goto EXIT;
    
EXIT:
    // TODO: error prints
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
}

struct jobnode * jobpool_active_dequeue(void) {
    struct jobnode * temp = NULL;

    if (pthread_spin_lock(&_jobpool.qlock) != 0) goto EXIT;

    if (_jobpool.queue_count > 0) {
        temp =  _jobpool.active_queue;
        _jobpool.active_queue->prev->next = temp->next;
        _jobpool.active_queue->next->prev = temp->prev;
        _jobpool.active_queue = temp->next;
    }
    _jobpool.queue_count -= 1;

    if (pthread_spin_unlock(&_jobpool.qlock) != 0) goto EXIT;

    return temp;
    
EXIT:
    // TODO: error prints
    exit(1); // spinlock taking only fails in case of a dead lock, no recovery for that case
}







// Yield
// ===========================================================================

// TODO: else case does not handle error


// Thread pool 
// =========================================================================


#if 0
void * theadpool_routine(NULL)
{
    while (1) {
        struct jobnode * ctx = jobpool_active_dequeue();
        if (NULL == ctx) { // TODO: Find a better mechanism(possibly signal based that no jobs are available)            
            usleep(10000); // TODO: replace usleep, since it's deprecated
            continue;
        }

        int ret = plugin_function(ctx);
        if (ret == CLOSE_CONNECTION) {
            close(sockfd);
            jobpool_free_release(ctx);
        } else { // Maintain connection
            jobpool_blocked_put(ctx);
        }
    }
}



void threadpool_init(int count) {
    pthread_t thread_id;
    s = pthread_create(&thread_id, NULL, &func_name, NULL);

}
#endif
