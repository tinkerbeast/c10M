#include "handler.h"

// local
#include "jobpool.h"
#include "server.h"
// libraries
#include <stdio.h>
#include <stdlib.h> 
// systems
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
// freestanding
#include <stdbool.h>
#include <stddef.h>



#define HANDLER_PARALLEL_LIMIT 4096



/******************************************************************************/
/* common */
/******************************************************************************/

static handler_state_e handler_common_blockio(int connector_socket)
{
    server_state_e state = SERVER_ERROR;
    struct server_http_request request = {0};
    int keep_alive = 0;

    // Note: SERVER_OK is not a valid returt code for process_request
    state = server_http_process_request(connector_socket, &request);
    switch (state) {
        case SERVER_ERROR:        
            break; // TODO: How to handle this?
        case SERVER_CLIENT_KEEPALIVE:
            keep_alive = 1;
            break;
        case SERVER_CLIENT_CLOSE_REQ:
            keep_alive = 0;
            break;
        case SERVER_CLIENT_CLOSED:
        case SERVER_CLIENT_ERROR:
        default:
            return HANDLER_ERROR;
    }


    state = server_http_process_response(connector_socket, &request);
    if (state != SERVER_OK) {
        return state;
    }

    return (keep_alive? HANDLER_TRACK_CONNECTOR: HANDLER_UNTRACK_CONNECTOR) ;
}


int handler_common_init(void *(*start_routine) (void *))
{
    pthread_t thread;
    pthread_attr_t attr;
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        perror("handler: common-init: init");
        return -1;
    }

    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (ret != 0) {
        // TODO: free up attr
        perror("handler: common-init: detach");
        return -1;
    }

    ret = pthread_create(&thread, &attr, start_routine, NULL);
    if (ret != 0) {
        // TODO: free up attr
        perror("handler: common-init: create");
        return -1;
    }

    ret = pthread_attr_destroy(&attr);
    if (ret != 0) {
        perror("handler: common-init: attr-destroy");
        // WARNING: Possible memory-leak by not handling error
    }

    return 0;
}


/******************************************************************************/
/* uniprocess */
/******************************************************************************/

static sig_atomic_t handler_run = true;

static void * handler_process_uniprocess(void * param)
{
    (void)param;
    
    while(handler_run) {
        // get a pending job
        struct jobnode * job = jobq_active_dequeue();
        if (NULL == job) {
            usleep(10000); // TODO: change deprecated api
            continue;
        }

        handler_state_e state = handler_common_blockio(job->sockfd);
        switch(state) {
            case HANDLER_TRACK_CONNECTOR:
                atomic_store(&job->state, JOB_BLOCKED);
                break;
            case HANDLER_UNTRACK_CONNECTOR:
            case HANDLER_ERROR:
            default:
                atomic_store(&job->state, JOB_DONE);
        }

    }

    return NULL;
}

handler_state_e handler_init_uniprocess(void)
{
    int ret = -1;

    printf("Uniprocess init\n");

    ret = handler_common_init(handler_process_uniprocess);

    return (ret == 0)? HANDLER_OK: HANDLER_ERROR;
}

handler_state_e handler_deinit_uniprocess(void)
{
    printf("Uniprocess deinit\n");

    handler_run = false;
    return HANDLER_OK;
}



#if 0
handler_state_e handler_process_uniprocess_blockio(int server_socket, int connector_socket)
{
    server_state_e state = SERVER_ERROR;

    (void)server_socket;

    state = handler_blockio(connector_socket);

    if (state == SERVER_CLIENT_KEEPALIVE) {
        return HANDLER_TRACK_CONNECTOR;
    } else {
        return HANDLER_UNTRACK_CONNECTOR;
    }
}
#endif

struct handler_lifecycle handler_uniprocess = {
    .init = handler_init_uniprocess,
    .deinit = handler_deinit_uniprocess
};


/******************************************************************************/
/* fork */
/******************************************************************************/
static void * handler_process_fork(void * param)
{
    (void)param;

    signal(SIGCHLD, SIG_IGN); // TODO: Ignore sigchild in a better way

    // TODO: warn: parallel limit is not enforced

    while(handler_run) {
        // get a pending job
        struct jobnode * job = jobq_active_dequeue();
        if (NULL == job) {
            usleep(10000); // TODO: change deprecated api
            continue;
        }   

        if (!fork()) { // this is the child process
            // TODO: close the server socket on the child side
            //close(server_socket); // child doesn't need the server

            // doesn't make sense for a process not to handle keep-alive
            handler_state_e state = HANDLER_ERROR;
            do {
                state = handler_common_blockio(job->sockfd);
            } while(state == HANDLER_TRACK_CONNECTOR); 

            exit(EXIT_SUCCESS); // TODO: handle error conditions
        } else { // this is the parent process
            // since keepalive is done in process context,
            // no need to keep the client socket open here            
            atomic_store(&job->state, JOB_DONE);
        }
    }

    return NULL; 
}

handler_state_e handler_init_fork(void)
{
    int ret = -1;

    ret = handler_common_init(handler_process_fork);

    return (ret == 0)? HANDLER_OK: HANDLER_ERROR;
}

handler_state_e handler_deinit_fork(void)
{
    handler_run = false;
    return HANDLER_OK;
}

#if 0
int handler_process_fork_blockio(int server_socket, int connector_socket)
{
    server_state_e state = SERVER_ERROR;

    signal(SIGCHLD, SIG_IGN); // TODO: Ignore sigchild in a better way

    // TODO: warn: parallel limit is not enforced

    if (!fork()) { // this is the child process
        close(server_socket); // child doesn't need the server

        // doesn't make sense for a process not to handle keep-alive
        do {
            state = handler_blockio(connector_socket);
        } while(state == SERVER_CLIENT_KEEPALIVE); 

        exit(EXIT_SUCCESS); // TODO: handle error conditions
    }

    return HANDLER_UNTRACK_CONNECTOR; // since keepalive is done in process context + blocking io
}
#endif

struct handler_lifecycle handler_fork = {
    .init = handler_init_fork,
    .deinit = handler_deinit_fork
};


/******************************************************************************/
/* ptthread */
/******************************************************************************/

#include <pthread.h>

void* handler_process_pthread_worker(void* param) {

    server_state_e state = SERVER_ERROR;
    int connector_socket = *((int*)param);
    free(param);

    do {
        state = handler_common_blockio(connector_socket);
    } while(state == SERVER_CLIENT_KEEPALIVE); 


    close(connector_socket);

    return NULL;
}

handler_state_e handler_process_pthread_blockio(int server_socket, int connector_socket)
{

    pthread_t thread;    
    void * param = NULL;
    
    (void)server_socket;
    
    param = malloc(sizeof(connector_socket) * 1); // TODO: check malloc return
    *((int*)param) = connector_socket;

    // TODO: warn: parallel limit is not enforced

    if (pthread_create(&thread, NULL, handler_process_pthread_worker, param)) {
        perror("ERROR creating thread.");
    }

    return HANDLER_UNTRACK_CONNECTOR; // since keepalive is done in thread context + blocking io
}

struct handler_lifecycle handler_pthread = {
    .init = handler_init_uniprocess,
    .deinit = handler_deinit_uniprocess
};


