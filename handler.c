// freestanding
#include <stddef.h>
// systems
#include <stdlib.h> 
#include <unistd.h>
#include <signal.h>
// libraries
#include <stdio.h>
// local
#include "server.h"

#include "handler.h"


#define HANDLER_PARALLEL_LIMIT 4096



/******************************************************************************/
/* common */
/******************************************************************************/

static server_state_e handler_blockio(int connector_socket)
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
            return state;
        default:
            return SERVER_ERROR;
    }


    state = server_http_process_response(connector_socket, &request);
    if (state != SERVER_OK) {
        return state;
    }

    return (keep_alive? SERVER_CLIENT_KEEPALIVE: SERVER_CLIENT_CLOSE_REQ) ;
}


/******************************************************************************/
/* uniprocess */
/******************************************************************************/

handler_state_e handler_init_uniprocess_blockio(void)
{
    return HANDLER_OK;
}

handler_state_e handler_deinit_uniprocess_blockio(void)
{
    return HANDLER_OK;
}

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

struct handler_lifecycle handler_uniprocess = {
    .init = handler_init_uniprocess_blockio,
    .process = handler_process_uniprocess_blockio,
    .deinit = handler_deinit_uniprocess_blockio
};


/******************************************************************************/
/* fork */
/******************************************************************************/

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

struct handler_lifecycle handler_fork = {
    .init = handler_init_uniprocess_blockio,
    .process = handler_process_fork_blockio,
    .deinit = handler_deinit_uniprocess_blockio
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
        state = handler_blockio(connector_socket);
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
    .init = handler_init_uniprocess_blockio,
    .process = handler_process_pthread_blockio,
    .deinit = handler_deinit_uniprocess_blockio
};


