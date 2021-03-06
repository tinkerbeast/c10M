#ifndef C10M_JOB__JOBPOOL_H_
#define C10M_JOB__JOBPOOL_H_

// == includes ==

#include <setjmp.h>
// freestanding
#include <stdatomic.h>
#include <stdbool.h>


#ifdef __cplusplus
namespace c10m_job {
#endif


typedef enum job_state_enum {
    JOB_UNINITED,
    JOB_QUEUED,
    JOB_BLOCKED,
    JOB_DONE
} job_state_e;



// primitive types

struct jobnode {
    int sockfd;
    struct jobnode * next;      // synchronised by jobpool qlock
    struct jobnode * prev;      // synchronised by jobpool qlock
    struct jobnode * nextfree;  // synchrnoised by jobpool flock
    _Atomic job_state_e state;  // synchronised by atomicity
    bool yielded;               // single threaded use only
    sigjmp_buf buf;             // single threaded use only
};


// macro and static-inline functions

void job_yieldable(struct jobnode *);

int job_yield(struct jobnode *, int);

#define job_yieldable(x) do { \
        if (x.yielded) siglongjmp(x.buf, 1); \
    } while(0)


#define job_yield(x, y)  if (sigsetjmp(x.buf, 0) == 0) { x.yielded = true; return y; } \
                     else { x.yielded = false; }

#ifdef __cplusplus
extern "C" {
#endif

// protoypes

int jobpool_init(int size);

#if 0
struct jobnode * jobpool_blocked_get(int sockfd);

struct jobnode * jobpool_blocked_delete(int sockfd);

int jobpool_blocked_put(int sockfd, struct jobnode * node);
#endif

struct jobnode * jobpool_get(int sockfd);

struct jobnode * jobpool_free_acquire(int sockfd);

void jobpool_free_release(int sockfd);

void jobq_active_enqueue(struct jobnode * job);

struct jobnode * jobq_active_dequeue(void);


#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif // namespace

#endif // C10M_JOB__JOBPOOL_H_






