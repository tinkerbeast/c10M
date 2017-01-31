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
#define TUPLE_TYPE TUPLE_INET
#define TUPLE_NODE NULL
#define TUPLE_SERVICE "4321"

/* CONFIG RESULT */
static struct TupleClass *tuple;


void conf(void) {

    if (TUPLE_INET == TUPLE_TYPE) {
        tuple = &tuple_inetsock;
    } else {
        fprintf(stderr, "conf: no matching tuple module");
    }
    tuple->node = TUPLE_NODE;
    tuple->service = TUPLE_SERVICE;
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

  rc = poll_select_blockio(server_sock, handler_fork.process);
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
