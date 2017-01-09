// freestanding
// system
// libraries
#include <stdlib.h>
#include <stdio.h>
// local
#include "tuple_socket_inet.h"
#include "handler.h"
#include "poll.h"

int main(int argc, char* argv[])
{
  int rc = -1;
  int server_sock = -1;

  (void)argc;
  (void)argv;

  rc = tuple_inetsock_create(&server_sock);
  if (rc != 0) {
    fprintf(stderr, "main: server-create failed");
    return EXIT_FAILURE;
  }

  rc = poll_select_blockio(server_sock, handler_fork.process);
  if (rc != 0) {
    fprintf(stderr, "main: server-poll failed");
    return EXIT_FAILURE;
  }

  rc = tuple_inetsock_delete(server_sock);
  if (rc != 0) {
    fprintf(stderr, "main: server-delete failed");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
