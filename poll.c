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

/******************************************************************************/
struct Poller {
    int (*init)(void* self, int server_socket);
    int (*process)(int server_socket, int connector_socket);
    int (*deinit)(void* self);
};


struct SelectPoller {
    fd_set read_fds;
    fd_set cached_read_fds;
    int fd_max_value;
    int fd_count;
    int server_socket;
    int iterator;
};

int  SelectPoller_init(struct SelectPoller* self, int server_socket)
{
    FD_ZERO(&self->read_fds);
    FD_ZERO(&self->cached_read_fds);
    FD_SET(server_socket, &self->read_fds);
    self->fd_max_value = server_socket;
    self->fd_count = 1;
    self->server_socket = server_socket;
    self->iterator = 0;

    return 0;
}

void  SelectPoller_deinit(struct SelectPoller* self)
{
    // nothing to do
    (void)self;
}

int  SelectPoller_wait(struct SelectPoller* self)
{
    // we mutate the list we read, so make a cache
    memcpy(&self->cached_read_fds, &self->read_fds, sizeof(fd_set));

    // TODO: Major TODO: Currently the read and write calls within the process callbacks
    //                   are blocking. We need to make them non-blockin for optimal use of
    //                   the select loop. Multi-part read and writes need to be done upon
    //                   muliple select events rather than looped blocking read/writes
    // Dev-Note: The looped blocking read write is different from handling keep-alive
    //           requests. The looped non-blocking multipart read/writes will still handle
    //           only one request/response pair. However the process level looping will
    //           determine keepalive connections.

    // Wait for event        
    return select(self->fd_max_value+1, &self->cached_read_fds, NULL, NULL, NULL);

}

int  SelectPoller_try_acceptfd(struct SelectPoller* self)
{
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
            return -1;
            //perror("poll-select: accept:");
        } else if (FD_SETSIZE > self->fd_count) { // accept connection
            //char *connector_add_str = NULL;
            FD_SET(connector_socket, &self->read_fds);
            self->fd_max_value = (self->fd_max_value < connector_socket)? connector_socket: self->fd_max_value;
            self->fd_count += 1;
            /* // commented for performance reasons
               connector_add_str = tuple_sockaddr_str((struct sockaddr *)&connector_addr, addr_str, sizeof(addr_str));
               printf("poll-select: connection-accepted:: %s\n", connector_add_str);  */
        } else { // can't accept any more connections
            return -1;
            //fprintf(stderr, "poll-select: fd-count:: read_fds can't accomodate more fds\n");
        }
    }

    return connector_socket;
}

void  SelectPoller_iterator_reset(struct SelectPoller* self)
{
    self->iterator = 0;
}

int  SelectPoller_iterator_getfd(struct SelectPoller* self)
{
    if (self->iterator > self->fd_max_value) {
        return -1;    
    }
    
    while (self->iterator == self->server_socket 
           || FD_ISSET(self->iterator, &self->cached_read_fds) == 0) {
        self->iterator += 1;
    }
    
    self->iterator += 1;    // increment the iterator for next step
    return (self->iterator - 1); // return the last value before increment
}

void  SelectPoller_releasefd(struct SelectPoller* self, int fd)
{
    FD_CLR(fd, &self->read_fds);
    self->fd_count -= 1;
    close(fd);
}

/******************************************************************************/
/* acceptloop */
/******************************************************************************/

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

int poll_select_blockio(int server_socket, handler_process_fn process_fn)
{

    int rc = -1;
    handler_state_e handler_state = HANDLER_ERROR;
    struct SelectPoller self;

    // listen 
    rc = listen(server_socket, POLL_CONNECTION_BACKLOG); // TODO: cleanup code
    if (rc == -1) {
        perror("poll-select: listen:");
        return -1;
    }
    printf("poll-select: listening:: On socket %d\n", server_socket);

    // init the poller
    rc = SelectPoller_init(&self, server_socket);
    if (rc == -1) {
        fprintf(stderr, "poll-select: poller_init:: Initialisation failed\n");
        return -1;
    }

    // hanlde server closing
    poll_sigint_hook(); // TODO: add cleanup code

    // selectloop
    while(poll_run) {

        // Wait for event
        rc = SelectPoller_wait(&self);
        if (rc == -1) { // TODO: WARN: OOB data is ignored
            perror("poll-select: poller_wait:");
            continue;
        }

        // Try accepting connection
        rc = SelectPoller_try_acceptfd(&self);
        if (rc == -1) {
            perror("poll-accept: poller_try_acceptfd:");
        }

        // run through the existing connections looking for data to read
        SelectPoller_iterator_reset(&self);
        int fd_iterator = SelectPoller_iterator_getfd(&self);
        while (fd_iterator > 0) {

            handler_state = process_fn(server_socket, fd_iterator);
            switch (handler_state) {
                case HANDLER_TRACK_CONNECTOR:
                    break;
                case HANDLER_ERROR:
                    fprintf(stderr, "poll-accept: process-error:: Closing connection\n");
                case HANDLER_UNTRACK_CONNECTOR:
                    SelectPoller_releasefd(&self, fd_iterator);
                    break;
                default:
                    fprintf(stderr, "poll-accept: process-illegal:: Terminatinf server\n");
                    // TODO: listen cleanup before exiting
                    return -1;
            }

            fd_iterator = SelectPoller_iterator_getfd(&self);
        }
    }

    SelectPoller_deinit(&self);
    printf("poll-accept: graceful exit\n");

    return 0;
}

// https://github.com/troydhanson/network/blob/master/tcp/server/sigio-server.c