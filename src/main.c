// freestanding6
// system
// libraries
#include <stdlib.h>
#include <stdio.h>
// local
#include "httpio/poll.h"
#include "httpio/handler.h"
#include "httpio/jobpool.h"
#include "httpio/tuple.h"

/* DEFAULT CONFIG */
#ifndef TUPLE_TYPE
#define TUPLE_TYPE TUPLE_INET
#endif

#ifndef IOLOOP_TYPE
#define IOLOOP_TYPE IOLOOP_SELECT
#endif

#ifndef HANDLER_LIFECYCLE 
#define HANDLER_LIFECYCLE PROCESS_THREADPOOL
#endif

#define TUPLE_NODE NULL
#define TUPLE_SERVICE "8888"

#define MAX_CON 10000


/* CONFIG RESULT */
static struct TupleClass tuple;
static struct handler_lifecycle handler;
static struct Poller ioloop_type;
static void *ioloop_inst = NULL;


void conf(void) {

    int ret = -1;

    // Assign tuple type based on config
    ret = tuple_class_get(TUPLE_TYPE, &tuple);
    if (ret != 0) {
        fprintf(stderr, "conf: no matching tuple module\n");
        exit(1);
    }
    tuple.node = TUPLE_NODE;
    tuple.service = TUPLE_SERVICE;

    // Assign ioloop based on config
    ret = ioloop_poller_get(IOLOOP_TYPE, &ioloop_type);
    if (ret != 0) {
        fprintf(stderr, "conf: no matching ioloop module\n");
        exit(1);
    }
    ioloop_inst = malloc(IOLOOP_INST_SIZE_MAX);
    if (ioloop_inst == NULL) {
      fprintf(stderr, "conf: ioloop_inst could not be allocated\n");
      exit(1);
    }

    // Assign process type based on config
    ret = handler_lifecycle_get(HANDLER_LIFECYCLE, &handler);
    if (ret != 0) {
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

  rc = tuple.create(&server_sock, tuple.node, tuple.service);
  if (rc != 0) {
    fprintf(stderr, "main: server-create failed");
    return EXIT_FAILURE;
  }
  
  rc = jobpool_init(MAX_CON);
  if (rc != 0) {
    fprintf(stderr, "main: jobpool-create failed");
    return EXIT_FAILURE;
  }

  rc = handler.init();
  if (rc != 0) {
    fprintf(stderr, "main: handler-create failed");
    return EXIT_FAILURE;
  }

  rc = poll_ioloop(server_sock, &ioloop_type, ioloop_inst);
  if (rc != 0) {
    fprintf(stderr, "main: server-poll failed");
    return EXIT_FAILURE;
  }

  printf("Exited ioloop cleanly\n");
  
  rc = handler.deinit();
  if (rc != 0) {
    fprintf(stderr, "main: handler-cleanup failed");
    return EXIT_FAILURE;
  }

  rc = tuple.delete(server_sock);
  if (rc != 0) {
    fprintf(stderr, "main: server-delete failed");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
