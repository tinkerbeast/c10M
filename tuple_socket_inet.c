// freestanding
#include <sys/types.h>
// systems
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
// libraries
#include <stdio.h>
#include <string.h>
// local

#include "tuple.h"





char *tuple_sockaddr_str(struct sockaddr *sa, char* dst, size_t dst_size)
{
  int rc = -1;
  char host[INET6_ADDRSTRLEN];
  char service[INET6_ADDRSTRLEN];
  socklen_t size = 0;
  
  if (sa->sa_family == AF_INET) {   
    size = sizeof(struct sockaddr_in);
  } else {
    size = sizeof(struct sockaddr_in6);
  }

  rc = getnameinfo(sa, size, host, INET6_ADDRSTRLEN, service, INET6_ADDRSTRLEN, 0);
  if (rc == 0) {
    snprintf(dst, dst_size, "%s:%s", host, service);
  } else {
    snprintf(dst, dst_size, "?:?");
  }

  return dst;
}


int tuple_inetsock_create(int *server_socket, const char *node, const char* service)
{
    int rc = -1;
    const int yes = 1;
    //const int no = 0;
    //char addr_str[INET6_ADDRSTRLEN];

    int server_sock = -1;
    struct addrinfo server_addr;
    struct addrinfo server_addr_hints;
    struct addrinfo *result_list = NULL;

    (void)node;
    

    // tuple info creation - hint for `protocol`, dst side
    memset(&server_addr_hints, 0, sizeof(server_addr_hints));
    // TODO: IPV6 support
    server_addr_hints.ai_family = AF_INET;    /* Allow IPv4 only */
    server_addr_hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    server_addr_hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    server_addr_hints.ai_protocol = 0;          /* Any protocol */

    // tuple info creation - create all possible tuples and fill dst_addr, dst_port
    rc = getaddrinfo(NULL, service, &server_addr_hints, &result_list);
    if (rc != 0) {
        fprintf(stderr, "server-create: getaddrinfo:: %s\n", gai_strerror(rc));
        return -1;
    }

    // tuple info filter - should be only one result based on the information passed
    if (result_list != NULL) {
        if (result_list->ai_next != NULL) {
            freeaddrinfo(result_list);
            fprintf(stderr, "server-create: addr-list:: More than one possible server address");
            return -1;
        } else {
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.ai_family = result_list->ai_family;
            server_addr.ai_socktype = result_list->ai_socktype;
            server_addr.ai_protocol = result_list->ai_protocol;
            server_addr.ai_addr = result_list->ai_addr;
            server_addr.ai_addrlen = result_list->ai_addrlen;
            freeaddrinfo(result_list);
        }
    }

    // tuple binding - `protocol` is binded to socket
    server_sock = socket(server_addr.ai_family, server_addr.ai_socktype, server_addr.ai_protocol);
    if (server_sock == -1) {
        perror("server-create: socket:");
        return -1;
    }

    // tuple binding - `dst_addr` wildcard 0.0.0.0 is to be undermined
    rc = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (rc == -1) {
        close(server_sock);
        perror("server-create: setsockopt:");
        return -1;
    }

    // tuple binding - `dst_addr`, `dst_port` is binded socket
    rc = bind(server_sock, server_addr.ai_addr, server_addr.ai_addrlen);
    if (rc == -1) {
        close(server_sock);
        perror("server-create: bind:");
        return -1;
    }

    /* // TODO: Commented for performance reasons
    printf("server-create: socket %d ready on %s\n", server_sock, sockaddr_inet_ntop(server_addr.ai_addr, addr_str, sizeof(addr_str)));
    */

    *server_socket = server_sock;

    return 0;
}


int tuple_inetsock_delete(int server_socket)
{
  return close(server_socket);
}


struct TupleClass tuple_inetsock = {
    .create = tuple_inetsock_create,
    .delete = tuple_inetsock_delete
};
