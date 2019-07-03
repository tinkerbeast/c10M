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

typedef enum sock_state_enum {
    SOCK_READABLE,
    SOCK_WRITABLE,
    SOCK_OOB,
    SOCK_SHUTDOWN,
    SOCK_UNKNOWN
} sock_state_e;




// aggregate types

struct Poller {
    int (*init)(void* self, int server_socket);
    void (*deinit)(void* self);
    int (*wait)(void* self);
    int (*try_acceptfd)(void* self, int * sockfd);
    void (*iterator_reset)(void* self);
    int (*iterator_getfd)(void* self, sock_state_e * state);
    void (*releasefd)(void* self, int fd);
    int (*maxfd)(void* self);
};


#ifdef __cplusplus
extern "C" {
#endif

// protoypes

int poll_ioloop(int server_socket, struct Poller * poller_class, void * poller_inst);


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
