#ifndef C10M_IOLOOP__POLL_H_
#define C10M_IOLOOP__POLL_H_

#include "handler.h"

#ifdef __cplusplus
namespace c10m_ioloop {
#endif

#define IOLOOP_INST_SIZE_MAX (512)

// primitive types

typedef enum ioloop_type_enum {
   IOLOOP_ACCEPT,
   IOLOOP_SELECT,
   IOLOOP_POLL,
   IOLOOP_SIG,
   IOLOOP_EPOLL
} ioloop_type_e;

// aggregate types

struct Poller {
    int (*init)(void* self, int server_socket);
    void (*deinit)(void* self);
    int (*wait)(void* self);
    int (*try_acceptfd)(void* self);
    void (*iterator_reset)(void* self);
    int (*iterator_getfd)(void* self);
    void (*releasefd)(void* self, int fd);
};


#ifdef __cplusplus
extern "C" {
#endif

// protoypes

int poll_ioloop(int server_socket, handler_process_fn process_fn, struct Poller * poller_class, void * poller_inst);


// externs

extern struct Poller poller_accept;

extern struct Poller poller_select;



#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif // C10M_IOLOOP__POLL_H_
