// Job queue + Job pool
// ===========================================================================

// cstd
#include <stdlib.h>
// system
#include <pthread.h>
#include <setjmp.h>
#include <unistd.h>
// freestanding
#include <stdbool.h>

struct jobnode {
    int sockfd;
    sigjmp_buf buf;
    bool yielded;
    struct jobnode * next;
    struct jobnode * prev;
};

struct jobpool {
    struct jobnode * free_pool;
    struct jobnode * * blocking_map;
    struct jobnode * active_queue;
    int free_count;
    int queue_count;
    int size;
    pthread_spinlock_t flock; // DEVNOTE: Using spinlock since I don't want context switch in case of wait
    pthread_spinlock_t qlock; // DEVNOTE: Using spinlock since I don't want context switch in case of wait
} _jobpool = {NULL, NULL, NULL};

static inline void job_yieldable(struct jobnode *);
static inline int job_yield(struct jobnode *, int);

#define job_yieldable(x) do { \
        if (x.yielded) siglongjmp(x.buf, 1); \
    } while(0)


#define job_yield(x, y)  if (sigsetjmp(x.buf, 0) == 0) { x.yielded = true; return y; } \
                     else { x.yielded = false; }



int jobpool_init(int size) {
    struct jobnode * jobs = malloc(sizeof(struct jobnode) * size);
    if (NULL == jobs) { return -1; // TODO: errno print 
    }
    
    _jobpool.blocking_map = malloc(sizeof(struct jobnode *) * size);
    if (NULL == jobs) { return -1; // TODO: errno print 
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

    pthread_spin_init(&_jobpool.flock, PTHREAD_PROCESS_PRIVATE); // TODO: check return
    return pthread_spin_init(&_jobpool.qlock, PTHREAD_PROCESS_PRIVATE);
}

// WARN: SINGLE-THREADED USE ONLY
struct jobnode * jobpool_blocked_get(unsigned int sockfd) {
    if (sockfd > _jobpool.size) {
        exit(1); // TODO: serious unhandled case where sockfd exceeds MAX_CONNECTIONS
    }

    struct jobnode * temp = _jobpool.blocking_map[sockfd];
    _jobpool.blocking_map[sockfd] = NULL;

    return temp;
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
