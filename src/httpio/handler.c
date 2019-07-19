#define _GNU_SOURCE // needed for sched.h
#include "handler.h"

// local
#include "jobpool.h"
#include "server.h"
// libraries
#include <stdio.h>
#include <stdlib.h> 
// systems
#include <pthread.h>
#include <sched.h>
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


int handler_common_init(void *(*start_routine) (void *), int affinity)
{
    pthread_t thread;
    pthread_attr_t attr;
    cpu_set_t cpuset;
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

    if (affinity >= 0) {
        CPU_ZERO(&cpuset);
        CPU_SET(affinity, &cpuset);
        ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (ret != 0) {
            perror("handler: common-init: affinity");
            return -1;
        }
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

    printf("Started thread\n");
    
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
    
    printf("Stopped thread\n");

    return NULL;
}

handler_state_e handler_init_uniprocess(void)
{
    int ret = -1;

    printf("Uniprocess init\n");

    ret = handler_common_init(handler_process_uniprocess, -1);

    return (ret == 0)? HANDLER_OK: HANDLER_ERROR;
}

handler_state_e handler_deinit_uniprocess(void)
{
    printf("Uniprocess deinit\n");

    handler_run = false;
    return HANDLER_OK;
}

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

    ret = handler_common_init(handler_process_fork, -1);

    return (ret == 0)? HANDLER_OK: HANDLER_ERROR;
}

handler_state_e handler_deinit_fork(void)
{
    handler_run = false;
    return HANDLER_OK;
}

struct handler_lifecycle handler_fork = {
    .init = handler_init_fork,
    .deinit = handler_deinit_fork
};


/******************************************************************************/
/* ptthread */
/******************************************************************************/



handler_state_e handler_init_threadpool(void)
{

    int ret = -1;

    // Get total available cores
    long num_threads = sysconf(_SC_NPROCESSORS_ONLN); // get the number of cpus available
    if (num_threads < 0) {
        perror("Could not get the number of availble cores");
        return HANDLER_ERROR;
    }
    num_threads = (num_threads > 1)? num_threads - 1: num_threads; // reserve one thread for the ioloop if possible

    // Creae thread pool
    for (int i = 0; i < num_threads; i++) {
        ret = handler_common_init(handler_process_uniprocess, -1);
        if (ret != 0) {
            return HANDLER_ERROR;
        }    
    }

    return HANDLER_OK;
}


int handler_lifecycle_get(handler_lifecycle_e type, struct handler_lifecycle * hl)
{
    if  (type == PROCESS_UNIPROCESS) {
        hl->init = handler_init_uniprocess;
        hl->deinit = handler_deinit_uniprocess;
    } else if  (type == PROCESS_FORK) {
        hl->init = handler_init_fork;
        hl->deinit = handler_deinit_fork;
    } else if  (type == PROCESS_THREADPOOL) {
        hl->init = handler_init_threadpool;
        hl->deinit = handler_deinit_uniprocess;
    } else {
        return -1;
    }

    return 0;
}

