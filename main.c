// freestanding6
// system
// libraries
#include <stdlib.h>
#include <stdio.h>
// local
#include "poll.h"
#include "handler.h"
#include "jobpool.h"
#include "tuple.h"

/* CONFIG */
#include "conf.h"

#define MAX_CON 10000


/* CONFIG RESULT */
static struct TupleClass *tuple = NULL;
static struct handler_lifecycle *handler = NULL;
static void *ioloop_inst = NULL;
static struct Poller *ioloop_type = NULL;


void conf(void) {

    // Assign tuple type based on config
    if (TUPLE_INET == TUPLE_TYPE) {
        tuple = &tuple_inetsock;
    } else {
        fprintf(stderr, "conf: no matching tuple module\n");
        exit(1);
    }
    tuple->node = TUPLE_NODE;
    tuple->service = TUPLE_SERVICE;

    // Assign ioloop based on config
    switch (IOLOOP_TYPE) {
        case IOLOOP_ACCEPT:            
            ioloop_type = &poller_accept;
            break;
        case IOLOOP_SELECT:            
            ioloop_type = &poller_select;
            break;
        default:
            fprintf(stderr, "conf: no matching ioloop module\n");
            exit(1);
    }
    ioloop_inst = malloc(IOLOOP_INST_SIZE_MAX);
    if (ioloop_inst == NULL) {
      fprintf(stderr, "conf: ioloop_inst could not be allocated\n");
      exit(1);
    }

    // Assign process type based on config
    switch (HANDLER_LIFECYCLE) {
        case PROCESS_UNIPROCESS:
            handler = &handler_uniprocess;
            break;
        case PROCESS_FORK:
            handler = &handler_fork;
            break;
        case PROCESS_PTHREAD:
            handler = &handler_pthread;
            break;
        default:
            fprintf(stderr, "conf: no matching process module\n");
            exit(1);
    }
}    


int main(int argc, char* argv[])
{
  int rc = -1;
  int server_sock = -1;

  (void)argc;
  (void)argv;

  conf(); // TODO: elaborate

  rc = tuple->create(&server_sock, tuple->node, tuple->service);
  if (rc != 0) {
    fprintf(stderr, "main: server-create failed");
    return EXIT_FAILURE;
  }
  
  rc = jobpool_init(MAX_CON);
  if (rc != 0) {
    fprintf(stderr, "main: jobpool-create failed");
    return EXIT_FAILURE;
  }

  rc = handler->init();
  if (rc != 0) {
    fprintf(stderr, "main: handler-create failed");
    return EXIT_FAILURE;
  }

  rc = poll_ioloop(server_sock, ioloop_type, ioloop_inst);
  if (rc != 0) {
    fprintf(stderr, "main: server-poll failed");
    return EXIT_FAILURE;
  }

  printf("Exited ioloop cleanly\n");
  
  rc = handler->deinit();
  if (rc != 0) {
    fprintf(stderr, "main: handler-cleanup failed");
    return EXIT_FAILURE;
  }

  rc = tuple->delete(server_sock);
  if (rc != 0) {
    fprintf(stderr, "main: server-delete failed");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
