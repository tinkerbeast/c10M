#ifndef C10M_SERVER__HTTP_H_
#define C10M_SERVER__HTTP_H_

#ifdef __cplusplus
namespace c10m_server {
#endif


// primitive types
typedef enum server_state_enum {
    SERVER_ERROR = -2,
    SERVER_CLIENT_ERROR = -1,
    SERVER_OK = 0,
    SERVER_CLIENT_KEEPALIVE = 1,
    SERVER_CLIENT_CLOSED,
    SERVER_CLIENT_CLOSE_REQ
} server_state_e;

// aggregate types
struct server_http_request {
    int dummy;
};

// inlines

// prototypes      

server_state_e server_http_process_request(int connector_fd, struct server_http_request *request);

server_state_e server_http_process_response(int connector_fd, const struct server_http_request *request);


#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif // C10M_SERVER__HTTP_H_
