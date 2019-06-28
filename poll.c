// freestanding
#include <sys/types.h>
// systems
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
// libraries
#include <stdio.h>
#include <string.h>
// local
#include "handler.h"

#include "poll.h"


#define POLL_CONNECTION_BACKLOG 10


/******************************************************************************/
/* comon */
/******************************************************************************/

static sig_atomic_t poll_run = 1;

static void poll_sigint_handler(int signal)
{
    (void)signal;
    poll_run = 0;
}

static void poll_sigint_hook(void)
{
    struct sigaction sig_int_handler;

    memset(&sig_int_handler, 0, sizeof(sig_int_handler));
    sig_int_handler.sa_handler = poll_sigint_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;
    sigaction(SIGINT, &sig_int_handler, NULL);
}




int poll_ioloop(int server_socket, handler_process_fn process_fn, struct Poller * poller_class, void * poller_inst)
{

    int rc = -1;
    handler_state_e handler_state = HANDLER_ERROR;

    // listen 
    rc = listen(server_socket, POLL_CONNECTION_BACKLOG); // TODO: cleanup code
    if (rc == -1) {
        perror("poll: listen:");
        return -1;
    }
    printf("poll: listening:: On socket %d\n", server_socket);

    // init the poller
    rc = poller_class->init(poller_inst, server_socket);
    if (rc == -1) {
        fprintf(stderr, "poll: poller_init:: Initialisation failed\n");
        return -1;
    }

    // hanlde server closing
    poll_sigint_hook(); // TODO: add cleanup code

    // selectloop
    while(poll_run) {

        // Wait for event
        rc = poller_class->wait(poller_inst);
        if (rc == -1) { // TODO: WARN: OOB data is ignored
            perror("poll: poller_wait:");
            continue;
        }

        // Try accepting connection
        int client_sock = -1;
        rc = poller_class->try_acceptfd(poller_inst, &client_sock);
        if (rc == -1) {
            perror("poll: poller_try_acceptfd:");
        }

        // run through the existing connections looking for data to read
        sock_state_e sock_state = SOCK_UNKNOWN;
        poller_class->iterator_reset(poller_inst);
        int fd_iterator = poller_class->iterator_getfd(poller_inst, &sock_state);
        while (fd_iterator > 0) {

            handler_state = process_fn(server_socket, fd_iterator);
            switch (handler_state) {
                case HANDLER_TRACK_CONNECTOR:
                    break;
                case HANDLER_ERROR:
                    fprintf(stderr, "poll: process-error:: Closing connection\n");
                    __attribute__ ((fallthrough)); // TODO: handle gcc specific annotations
                case HANDLER_UNTRACK_CONNECTOR:
                    poller_class->releasefd(poller_inst, fd_iterator);
                    break;
                default:
                    fprintf(stderr, "poll: process-illegal:: Terminatinf server\n");
                    // TODO: listen cleanup before exiting
                    return -1;
            }

            fd_iterator = poller_class->iterator_getfd(poller_inst, &sock_state);
        }
    }

    poller_class->deinit(poller_inst);
    printf("poll: graceful exit\n");

    return 0;
}

/******************************************************************************/
/* accept - new */
/******************************************************************************/

struct AcceptPoller {
    int server_socket;
    int connector_socket;
};

// TODO check sizeof AcceptPoller against IOLOOP_INST_SIZE_MAX 

int AcceptPoller_init(void * this, int server_socket)
{
    struct AcceptPoller* self = this;
    
    self->server_socket = server_socket;
    self->connector_socket = -1;

    return 0;
}

void AcceptPoller_deinit(void * this)
{
    // nothing to do
    (void)this;
}

int AcceptPoller_wait(void * this)
{
    (void)this;

    return 0;
}

int AcceptPoller_try_acceptfd(void * this, int * sockfd)
{
    struct sockaddr_storage connector_addr;
    socklen_t connector_addr_size = 0;

    struct AcceptPoller* self = this;
    connector_addr_size = sizeof(connector_addr);

    self->connector_socket = accept(self->server_socket, (struct sockaddr *)&connector_addr, &connector_addr_size);
    if (self->connector_socket == -1) {
        return -1;
    } else {
        *sockfd = self->connector_socket;
        return 0;
    }
}

void AcceptPoller_iterator_reset(void * this)
{
    // nothing to do
    (void)this;
}

int AcceptPoller_iterator_getfd(void * this, sock_state_e * sock_state)
{
    struct AcceptPoller* self = this;

    int temp = self->connector_socket;
    
    self->connector_socket = -1;

    *sock_state = SOCK_READABLE; // TODO: blindly setting readable might not work in all cases

    return temp;
}

void AcceptPoller_releasefd(void * this, int fd)
{
    (void)this;

    close(fd);
}

struct Poller poller_accept = {
    .init = AcceptPoller_init,
    .deinit = AcceptPoller_deinit,
    .wait = AcceptPoller_wait,
    .try_acceptfd = AcceptPoller_try_acceptfd,
    .iterator_reset = AcceptPoller_iterator_reset,
    .iterator_getfd = AcceptPoller_iterator_getfd,
    .releasefd = AcceptPoller_releasefd
};

/******************************************************************************/
/* select - new */
/******************************************************************************/

struct SelectPoller {
    fd_set all_fds;
    fd_set cached_read_fds;
    fd_set cached_write_fds;
    int fd_max_value;
    int fd_count;
    int server_socket;
    int iterator;
};

// TODO check sizeof SelectPoller against IOLOOP_INST_SIZE_MAX 

int SelectPoller_init(void * this, int server_socket)
{
    struct SelectPoller* self = this;

    FD_ZERO(&self->all_fds);

    FD_ZERO(&self->cached_read_fds);
    FD_ZERO(&self->cached_write_fds);

    FD_SET(server_socket, &self->all_fds);
    self->fd_max_value = server_socket;
    self->fd_count = 1;
    self->server_socket = server_socket;
    self->iterator = 0;

    return 0;
}

void SelectPoller_deinit(void * this)
{
    // nothing to do
    (void)this;
}

int SelectPoller_wait(void * this)
{
    struct SelectPoller* self = this;

    // we mutate the list we read, so make a cache
    memcpy(&self->cached_read_fds, &self->all_fds, sizeof(fd_set));
    memcpy(&self->cached_write_fds, &self->all_fds, sizeof(fd_set));

    // TODO: Major TODO: Currently the read and write calls within the process callbacks
    //                   are blocking. We need to make them non-blockin for optimal use of
    //                   the select loop. Multi-part read and writes need to be done upon
    //                   muliple select events rather than looped blocking read/writes
    // Dev-Note: The looped blocking read write is different from handling keep-alive
    //           requests. The looped non-blocking multipart read/writes will still handle
    //           only one request/response pair. However the process level looping will
    //           determine keepalive connections.

    // Wait for event
    // TODO: out of band data is not considered, which would have appeared as part of the except fd set
    return select(self->fd_max_value+1, 
            &self->cached_read_fds, &self->cached_write_fds, NULL, NULL);

}

int SelectPoller_try_acceptfd(void * this, int * sockfd)
{
    struct SelectPoller* self = this;

    socklen_t connector_addr_size = 0;
    int connector_socket = -1;
    struct sockaddr_storage connector_addr; // connector's address information

    if (FD_ISSET(self->server_socket, &self->cached_read_fds)) {

        // try to accept a connection
        connector_addr_size = sizeof(connector_addr);
        connector_socket = accept(self->server_socket, 
                (struct sockaddr *)&connector_addr, &connector_addr_size);

        // limits to accepting connection
        if (connector_socket == -1) { // error case
            perror("poll-select: accept:");
            return -1;            
        } else if (FD_SETSIZE > self->fd_count) { // accept connection
            //char *connector_add_str = NULL;
            FD_SET(connector_socket, &self->all_fds);
            self->fd_max_value = (self->fd_max_value < connector_socket)? connector_socket: self->fd_max_value;
            self->fd_count += 1;
            *sockfd = connector_socket;
            /* // commented for performance reasons
               connector_add_str = tuple_sockaddr_str((struct sockaddr *)&connector_addr, addr_str, sizeof(addr_str));
               printf("poll-select: connection-accepted:: %s\n", connector_add_str);  */
            return 0;
        } else { // can't accept any more connections
            fprintf(stderr, "poll-select: fd-count:: read_fds can't accomodate more fds\n");
            return -1;            
        }
    } else {
        return 0;
    }
}

void SelectPoller_iterator_reset(void * this)
{
    struct SelectPoller* self = this;

    self->iterator = 0;
}

int SelectPoller_iterator_getfd(void * this, sock_state_e * sock_state)
{
    struct SelectPoller* self = this;

    if (self->iterator > self->fd_max_value) {
        return -1;    
    }

    int readable = FD_ISSET(self->iterator, &self->cached_read_fds);
    int writeable = FD_ISSET(self->iterator, &self->cached_write_fds);
    while (self->iterator == self->server_socket || readable == 0 || writeable == 0) {
        self->iterator += 1;
        readable = FD_ISSET(self->iterator, &self->cached_read_fds);
        writeable = FD_ISSET(self->iterator, &self->cached_write_fds);
    }

    sock_state_e temp = SOCK_UNKNOWN;
    temp = readable? SOCK_READABLE : temp;
    temp = writeable? SOCK_WRITABLE : temp;
    temp = (readable && writeable)? SOCK_SHUTDOWN : temp;
    *sock_state = temp;
    
    self->iterator += 1;    // increment the iterator for next step
    return (self->iterator - 1); // return the last value before increment
}

void SelectPoller_releasefd(void * this, int fd)
{
    struct SelectPoller* self = this;

    FD_CLR(fd, &self->all_fds);
    self->fd_count -= 1;
    close(fd);
}

struct Poller poller_select = {
    .init = SelectPoller_init,
    .deinit = SelectPoller_deinit,
    .wait = SelectPoller_wait,
    .try_acceptfd = SelectPoller_try_acceptfd,
    .iterator_reset = SelectPoller_iterator_reset,
    .iterator_getfd = SelectPoller_iterator_getfd,
    .releasefd = SelectPoller_releasefd
};


/***********************************************************************************/

#if 0
struct SigIoPoller {

};

int SigIoPoller_init(void * this, int server_socket)
{
    struct SigIoPoller* self = this;

    /* Establish handler for "I/O possible" signal */
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = sigioHandler;
    if (sigaction(SIGIO, &sa, NULL) == -1)
        errExit("sigaction");
    //
    sigset_t sigio_set;
    sigemptyset( &sigio_set );
    sigaddset( &sigio_set, SIGALRM);
    sigprocmask(SIGIO, &sigio_set, NULL);
    /* Set owner process that is to receive "I/O possible" signal */
    if (fcntl(server_socket, F_SETOWN, getpid()) == -1)
        errExit("fcntl(F_SETOWN)");
    /* Enable "I/O possible" signaling and make I/O nonblocking
    for file descriptor */
    flags = fcntl(server_socket, F_GETFL);
    if (fcntl(server_socket, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1)
        errExit("fcntl(F_SETFL)");

    return 0;
}
#endif

/******************************************************************************/
/* acceptloop */
/******************************************************************************/
#if 0
int poll_acceptloop_blockio(int server_socket, handler_process_fn process_fn)
{
    int rc = -1;
    handler_state_e handler_state = HANDLER_ERROR;
    int connector_socket = -1;
    struct sockaddr_storage connector_addr; // connector's address information
    socklen_t connector_addr_size = 0;
    //char addr_str[INET6_ADDRSTRLEN]; // TODO: Remove inet6 reference

    // listen 
    rc = listen(server_socket, POLL_CONNECTION_BACKLOG);
    if (rc == -1) {
        perror("poll-accept: listen:");
        return -1;
    }
    printf("poll-accept: listening:: On socket %d\n", server_socket);

    // hanlde server closing
    poll_sigint_hook(); // TODO: add cleanup code

    // acceptloop
    while (poll_run) {
        //char *connector_add_str = NULL;

        // accept a connection
        // TODO: handle OOB connections
        connector_addr_size = sizeof(connector_addr);
        connector_socket = accept(server_socket, (struct sockaddr *)&connector_addr, &connector_addr_size);
        if (connector_socket == -1) {
            perror("poll-accept: accept:");
            continue;
        }
        /* // commented for performance reasons
           connector_add_str = tuple_sockaddr_str((struct sockaddr *)&connector_addr, addr_str, sizeof(addr_str));
           printf("poll-accept: connection-accepted:: %s\n", connector_add_str);  */

        do {
            handler_state = process_fn(server_socket, connector_socket);
            switch (handler_state) {
                case HANDLER_TRACK_CONNECTOR:
                    break;
                case HANDLER_ERROR:
                    fprintf(stderr, "poll-accept: process-error:: Closing connection\n");
                case HANDLER_UNTRACK_CONNECTOR:
                    close(connector_socket);
                    break;
                default:
                    fprintf(stderr, "poll-accept: process-illegal:: Terminatinf server\n");
                    // TODO: listen cleanup before exiting
                    return -1;          
            }
        } while (handler_state == HANDLER_TRACK_CONNECTOR);
    }

    printf("poll-accept: graceful exit\n");

    return 0;
}
#endif
/******************************************************************************/
/* select */
/******************************************************************************/

#if 0
int poll_select_blockio(int server_socket, handler_process_fn process_fn)
{
    int rc = -1;
    handler_state_e handler_state = HANDLER_ERROR;
    fd_set read_fds;
    fd_set cached_read_fds;
    int fd_max_value = -1;
    int fd_count = 0;
    int fd_iterator = -1;
    int connector_socket = -1;
    struct sockaddr_storage connector_addr; // connector's address information
    socklen_t connector_addr_size = 0;

    // listen 
    rc = listen(server_socket, POLL_CONNECTION_BACKLOG); // TODO: cleanup code
    if (rc == -1) {
        perror("poll-select: listen:");
        return -1;
    }
    printf("poll-select: listening:: On socket %d\n", server_socket);

    // clear the sets and add the server socket to the read set
    FD_ZERO(&read_fds);
    FD_ZERO(&cached_read_fds);
    FD_SET(server_socket, &read_fds);
    fd_max_value = server_socket;
    fd_count++;

    // hanlde server closing
    poll_sigint_hook(); // TODO: add cleanup code

    // selectloop
    while(poll_run) {

        // we mutate the list we read, so make a cache
        memcpy(&cached_read_fds, &read_fds, sizeof(fd_set));

        // TODO: Major TODO: Currently the read and write calls within the process callbacks
        //                   are blocking. We need to make them non-blockin for optimal use of
        //                   the select loop. Multi-part read and writes need to be done upon
        //                   muliple select events rather than looped blocking read/writes
        // Dev-Note: The looped blocking read write is different from handling keep-alive
        //           requests. The looped non-blocking multipart read/writes will still handle
        //           only one request/response pair. However the process level looping will
        //           determine keepalive connections.

        // Wait for event        
        rc = select(fd_max_value+1, &cached_read_fds, NULL, NULL, NULL);
        if (rc == -1) { // TODO: WARN: OOB data is ignored
            perror("poll-select: select:");
            continue;
        }

        // Event on the server socket
        if (FD_ISSET(server_socket, &cached_read_fds)) {

            // try to accept a connection
            connector_addr_size = sizeof(connector_addr);
            connector_socket = accept(server_socket, (struct sockaddr *)&connector_addr, &connector_addr_size);

            // limits to accepting connection
            if (connector_socket == -1) { // error case
                perror("poll-select: accept:");
            } else if (FD_SETSIZE > fd_count) { // accept connection
                //char *connector_add_str = NULL;
                FD_SET(connector_socket, &read_fds);
                fd_max_value = (fd_max_value < connector_socket)? connector_socket: fd_max_value;
                fd_count++;
                /* // commented for performance reasons
                   connector_add_str = tuple_sockaddr_str((struct sockaddr *)&connector_addr, addr_str, sizeof(addr_str));
                   printf("poll-select: connection-accepted:: %s\n", connector_add_str);  */
            } else { // can't accept any more connections
                fprintf(stderr, "poll-select: fd-count:: read_fds can't accomodate more fds\n");
            }
        }

        // run through the existing connections looking for data to read
        for(fd_iterator = 0; fd_iterator <= fd_max_value; fd_iterator++) {

            // fd is server socket or there is no event on fd
            if (fd_iterator == server_socket || FD_ISSET(fd_iterator, &cached_read_fds) == 0) {
                continue;
            }

            handler_state = process_fn(server_socket, fd_iterator);
            switch (handler_state) {
                case HANDLER_TRACK_CONNECTOR:
                    break;
                case HANDLER_ERROR:
                    fprintf(stderr, "poll-accept: process-error:: Closing connection\n");
                case HANDLER_UNTRACK_CONNECTOR:
                    FD_CLR(fd_iterator, &read_fds);
                    fd_count--;
                    close(fd_iterator);
                    break;
                default:
                    fprintf(stderr, "poll-accept: process-illegal:: Terminatinf server\n");
                    // TODO: listen cleanup before exiting
                    return -1;          
            }
        }
    }

    printf("poll-accept: graceful exit\n");

    return 0;
}
#endif

#if 0
int poll_select_blockio(int server_socket, handler_process_fn process_fn)
{
    struct SelectPoller self;
    //struct AcceptPoller self2;

    return poll_ioloop(server_socket, process_fn, &poller_select, &self);
}
#endif

// https://github.com/troydhanson/network/blob/master/tcp/server/sigio-server.c
