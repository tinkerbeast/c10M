#define _GNU_SOURCE // because of accept4
#include "poll.h"

// local
#include "jobpool.h"
#include "handler.h"
// stdlib
#include <stdio.h>
#include <string.h>
// systems
#include <signal.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
// freestanding
#include <stdlib.h>
#include <sys/types.h>


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



int poll_ioloop(int server_socket, struct Poller * poller_class, void * poller_inst)
{

    int rc = -1;

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
    //int lll = 0;
    while(poll_run) {
        /*
        printf("Loop: %d\n", lll);
        if (lll > 3) {
            sleep(1);
        }
        lll += 1;
        */

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
        } else if(client_sock == -1) {
            // no errors, but no sockets to accept
        } else {
            struct jobnode* job = jobpool_free_acquire(client_sock); // DEVNOTE: socket set, job-state changed
            if (NULL == job) {
                perror("poll: poller_try_acceptfd: job creation");
                close(client_sock); // TODO: check close return
            } else {    
                atomic_store(&job->state, JOB_BLOCKED); // Note: Atomic not really necessary
            }
        }

        // run through the existing connections looking for data to read
        sock_state_e sock_state = SOCK_UNKNOWN;
        poller_class->iterator_reset(poller_inst);
        int fd_iterator = poller_class->iterator_getfd(poller_inst, &sock_state);
        while (fd_iterator > 0) {

            //if (sock_state == SOCK_SHUTDOWN) {
            if (0) {

                // TODO TODO TODO TODO
                // TODO: add remote shutdown case based sock_state
            } else {
                struct jobnode * job = jobpool_get(fd_iterator);
                if (NULL == job) {
                    fprintf(stderr, "poll: illegal-state:: Terminating server sock=%d, state=%d\n", fd_iterator, sock_state);
                    // TODO: listen cleanup before exiting
                    return -1;
                }
                
                //printf("Before enqueue state: %d\n", job->state);
                job_state_e expected = JOB_BLOCKED;
                if (atomic_compare_exchange_strong(&job->state, &expected, JOB_QUEUED)) {
                    // TODO TODO TODO TODO
                    // TODO: must remove job from select fds
                    jobq_active_enqueue(job);
                    //printf("enqueueq\n");
                }
            }

#if 0            
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
#endif            

            fd_iterator = poller_class->iterator_getfd(poller_inst, &sock_state);
        }

        // TODO TODO TODO TODO
        // TODO This is inefficient, we can just have a queue for cleanup
        // Cleanup all fds marked for cleanup
        int max_fd = poller_class->maxfd(poller_inst);
        for (int sockfd = server_socket; sockfd <= max_fd; sockfd++) {
            job_state_e expected = JOB_DONE;
            struct jobnode * job = jobpool_get(sockfd);
            if (NULL == job) {
                continue;
            } else if (atomic_compare_exchange_strong(&job->state, &expected, JOB_UNINITED)) {
                poller_class->releasefd(poller_inst, sockfd);
                close(sockfd); // TODO: check returns
                jobpool_free_release(sockfd);
            }
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
    int fd_max_value;
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

    self->connector_socket = accept4(self->server_socket, (struct sockaddr *)&connector_addr, 
                                    &connector_addr_size, SOCK_NONBLOCK);
    if (self->connector_socket == -1) {
        return -1;
    } else {
        *sockfd = self->connector_socket;
        self->fd_max_value = (self->fd_max_value < self->connector_socket)? self->connector_socket: self->fd_max_value;        
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
    struct AcceptPoller* self = this;

    if (fd == self->fd_max_value) {
        self->fd_max_value -= 1;
    }
}

int AcceptPoller_maxfd(void * this)
{
    struct AcceptPoller* self = this;

    return self->fd_max_value;
}

struct Poller poller_accept = {
    .init = AcceptPoller_init,
    .deinit = AcceptPoller_deinit,
    .wait = AcceptPoller_wait,
    .try_acceptfd = AcceptPoller_try_acceptfd,
    .iterator_reset = AcceptPoller_iterator_reset,
    .iterator_getfd = AcceptPoller_iterator_getfd,
    .releasefd = AcceptPoller_releasefd,
    .maxfd =  AcceptPoller_maxfd
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
        connector_socket = accept4(self->server_socket, 
                (struct sockaddr *)&connector_addr, &connector_addr_size, SOCK_NONBLOCK);

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
        if (self->iterator > self->fd_max_value) {
            return -1;    
        }
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
    if (self->fd_max_value == fd) {
        self->fd_max_value -= 1;
    }
}

int SelectPoller_maxfd(void * this)
{
    struct SelectPoller* self = this;

    return self->fd_max_value;
}

struct Poller poller_select = {
    .init = SelectPoller_init,
    .deinit = SelectPoller_deinit,
    .wait = SelectPoller_wait,
    .try_acceptfd = SelectPoller_try_acceptfd,
    .iterator_reset = SelectPoller_iterator_reset,
    .iterator_getfd = SelectPoller_iterator_getfd,
    .releasefd = SelectPoller_releasefd,
    .maxfd = SelectPoller_maxfd
};


/******************************************************************************/
/* epoll */
/******************************************************************************/

struct EpollPoller { 
    struct epoll_event * epoll_events;
    uint32_t max_events;
    uint32_t iterator_nfds;
    uint32_t iterator_cur;
    int epollfd;
    int server_socket;
};

int EpollPoller_init(void * this, int server_socket, int max_connections)
{
    struct EpollPoller* self = this;

    self->epoll_events = malloc(max_connections * sizeof(struct EpollPoller));
    if (NULL == self->epoll_events) {
        return -1;
    }

    self->max_events = max_connections;

    self->epollfd = epoll_create(max_connections);
    if (self->epollfd < 0) {
        free(self->epoll_events);
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_socket;
    if (epoll_ctl(self->epollfd, EPOLL_CTL_ADD, server_socket, &ev) == -1) {
        return -1;
    }
    self->server_socket = server_socket;

    return 0;
}

void EpollPoller_deinit(void * this)
{
    struct EpollPoller* self = this;

    free(self->epoll_events);
}

int EpollPoller_wait(void * this)
{
    struct EpollPoller* self = this;

    self->iterator_nfds = epoll_wait(self->epollfd, self->epoll_events, self->max_events, -1);
    // TODO return check

    return 0;
}

// TODO: for epoll this will called invariant of connection of server socket, need to optimize
int EpollPoller_try_acceptfd(void * this, int * sockfd)
{
    struct EpollPoller* self = this;

    struct sockaddr_storage connector_addr;
    socklen_t connector_addr_size = sizeof(connector_addr);

    int connector_socket = accept4(self->server_socket, (struct sockaddr *)&connector_addr, 
                                    &connector_addr_size, SOCK_NONBLOCK);
    if (connector_socket == -1) {
        return -1;
    } 

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    if (epoll_ctl(self->epollfd, EPOLL_CTL_ADD, connector_socket, &ev) == -1) {
        close(connector_socket); // TODO: check return of close
        return -1;
    }
    
    *sockfd = connector_socket;

    return 0;
}

void EpollPoller_iterator_reset(void * this)
{
    struct EpollPoller* self = this;

    self->iterator_cur = 0;
}

int EpollPoller_iterator_getfd(void * this, sock_state_e * sock_state)
{
    struct EpollPoller* self = this;

    if (self->iterator_cur == self->server_socket) {
        self->iterator_cur += 1;
    }

    if (self->iterator_cur >= self->iterator_nfds) {
        return -1; 
    }

    struct epoll_event * event = &self->epoll_events[self->iterator_cur];
    
    sock_state_e temp = SOCK_UNKNOWN;
    temp = (event->events & EPOLLIN) ? SOCK_READABLE : temp;
    temp = (event->events & EPOLLOUT) ? SOCK_WRITABLE : temp;
    temp = (event->events & EPOLLRDHUP) ? SOCK_SHUTDOWN : temp;
    *sock_state = temp;

    self->iterator_cur += 1;    // increment the iterator for next step
    return event->data.fd; 
}

void EpollPoller_releasefd(void * this, int fd)
{
    struct EpollPoller* self = this;

    if (epoll_ctl(self->epollfd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        // TODO print error properly
        perror("Error when removing fd from control group");
    }
}

/***********************************************************************************/

int ioloop_poller_get(ioloop_type_e type, struct Poller * pl)
{
    if  (type == IOLOOP_ACCEPT) {
        pl->init = AcceptPoller_init;
        pl->deinit = AcceptPoller_deinit;
        pl->wait = AcceptPoller_wait;
        pl->try_acceptfd = AcceptPoller_try_acceptfd;
        pl->iterator_reset = AcceptPoller_iterator_reset;
        pl->iterator_getfd = AcceptPoller_iterator_getfd;
        pl->releasefd = AcceptPoller_releasefd;
        pl->maxfd =  AcceptPoller_maxfd;
    } else if  (type == IOLOOP_SELECT) {
        pl->init            = SelectPoller_init;
        pl->deinit          = SelectPoller_deinit;
        pl->wait            = SelectPoller_wait;
        pl->try_acceptfd    = SelectPoller_try_acceptfd;
        pl->iterator_reset  = SelectPoller_iterator_reset;
        pl->iterator_getfd  = SelectPoller_iterator_getfd;
        pl->releasefd       = SelectPoller_releasefd;
        pl->maxfd           = SelectPoller_maxfd;
    } else if  (type == IOLOOP_EPOLL) {
        pl->init            = EpollPoller_init;
        pl->deinit          = EpollPoller_deinit;
        pl->wait            = EpollPoller_wait;
        pl->try_acceptfd    = EpollPoller_try_acceptfd;
        pl->iterator_reset  = EpollPoller_iterator_reset;
        pl->iterator_getfd  = EpollPoller_iterator_getfd;
        pl->releasefd       = EpollPoller_releasefd;
        pl->maxfd           = EpollPoller_maxfd;
    } else {
        return -1;
    }

    return 0;
}

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


// https://github.com/troydhanson/network/blob/master/tcp/server/sigio-server.c
