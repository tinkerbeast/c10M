//local
#include "c10m_socket.h"
// freestanding
// system
#include <netinet/in.h>
// libraries
#include <cstdlib>
#include <cstdio>

int main(int argc, char* argv[])
{
  (void)argc;
  (void)argv;

  
  std::string node = "0.0.0.0";
  std::string service = "8888";
  std::vector<c10m::socket::sockopt> options;
  int backlog = 10;

  c10m::socket::InetServer server(node, service, IPPROTO_TCP, options, backlog);
 
  server.open();

  c10m::io::IoInterface* client = server.peer_establish();

  client->close();

  server.close();

  return EXIT_SUCCESS;
}
