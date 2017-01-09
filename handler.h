
// primitive types

typedef enum handler_state_enum {
   HANDLER_ERROR = -1,
   HANDLER_OK = 0,
   HANDLER_TRACK_CONNECTOR,
   HANDLER_UNTRACK_CONNECTOR
} handler_state_e; 

// function pointers

typedef handler_state_e (*handler_process_fn)(int server_socket, int connector_socket);

// aggregate types

struct handler_lifecycle {
    handler_state_e (*init)(void);
    handler_state_e (*process)(int server_socket, int connector_socket);
    handler_state_e (*deinit)(void);
};

// inlines

// prototypes

// externs

extern struct handler_lifecycle handler_uniprocess;

extern struct handler_lifecycle handler_fork;
