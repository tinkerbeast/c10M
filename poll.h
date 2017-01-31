#ifndef C10M_IOLOOP__POLL_H_
#define C10M_IOLOOP__POLL_H_

#ifdef __cplusplus
namespace c10m_ioloop {
#endif

#ifdef __cplusplus
extern "C" {
#endif

// protoypes

int poll_acceptloop_blockio(int server_socket, handler_process_fn process_fn);

int poll_select_blockio(int server_socket, handler_process_fn process_fn);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif // C10M_IOLOOP__POLL_H_
