// freestanding
// system
// libraries
#include <stdlib.h>
#include <stdio.h>
// local
#include "tuple.h"
#include "handler.h"
#include "poll.h"

/* CONFIG */
#ifndef TUPLE_TYPE
#define TUPLE_TYPE TUPLE_INET
#endif

#ifndef HANDLER_LIFECYCLE 
#define HANDLER_LIFECYCLE PROCESS_UNIPROCESS
#endif

#define TUPLE_NODE NULL
#define TUPLE_SERVICE "8888"

/* CONFIG RESULT */
static struct TupleClass *tuple;
static struct handler_lifecycle *handler;


void conf(void) {

    if (TUPLE_INET == TUPLE_TYPE) {
        tuple = &tuple_inetsock;
    } else {
        fprintf(stderr, "conf: no matching tuple module");
        exit(1);
    }

    tuple->node = TUPLE_NODE;
    tuple->service = TUPLE_SERVICE;

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
            fprintf(stderr, "conf: no matching tuple module");
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

  rc = poll_select_blockio(server_sock, handler->process);
  if (rc != 0) {
    fprintf(stderr, "main: server-poll failed");
    return EXIT_FAILURE;
  }

  rc = tuple->delete(server_sock);
  if (rc != 0) {
    fprintf(stderr, "main: server-delete failed");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
