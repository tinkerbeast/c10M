#ifndef C10M_JOB__JOBPOOL_H_
#define C10M_JOB__JOBPOOL_H_

// == includes ==

#include <setjmp.h>
// freestanding
#include <stdbool.h>


#ifdef __cplusplus
namespace c10m_job {
#endif


// primitive types

struct jobnode {
    int sockfd;
    sigjmp_buf buf;
    bool yielded;
    struct jobnode * next;
    struct jobnode * prev;
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

struct jobnode * jobpool_blocked_get(int sockfd);

struct jobnode * jobpool_free_acquire(void);

void jobpool_free_release(struct jobnode* released);

void jobpool_active_enqueue(struct jobnode * job);

struct jobnode * jobpool_active_dequeue(void);


#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif // namespace

#endif // C10M_JOB__JOBPOOL_H_






