#ifndef C10M_WORKER__HANDLER_H_
#define C10M_WORKER__HANDLER_H_


#ifdef __cplusplus
namespace c10m_worker {
#endif


// primitive types

typedef enum handler_state_enum {
   HANDLER_ERROR = -1,
   HANDLER_OK = 0,
   HANDLER_TRACK_CONNECTOR,
   HANDLER_UNTRACK_CONNECTOR
} handler_state_e;


typedef enum handler_lifecycle_enum {
   PROCESS_UNIPROCESS,
   PROCESS_FORK,
   PROCESS_THREADPOOL
} handler_lifecycle_e;


// function pointers

typedef handler_state_e (*handler_process_fn)(int server_socket, int connector_socket);

// aggregate types

struct handler_lifecycle {
    handler_state_e (*init)(void);
    handler_state_e (*deinit)(void);
};

// inlines

#ifdef __cplusplus
extern "C" {
#endif
// prototypes

// externs

extern struct handler_lifecycle handler_uniprocess;

extern struct handler_lifecycle handler_fork;

extern struct handler_lifecycle handler_threadpool;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif // C10M_WORKER__HANDLER_H_

