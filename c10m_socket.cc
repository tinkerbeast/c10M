// counterpart
#include "c10m_socket.h"
// local
#include "c10m_os.h"
// freestanding
// systems
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
// libraries
#include <cerrno>
#include <map>
#include <iostream>


// note to self for java like inheritance, follow these rules:
//  * single class inheritance only (avoids resolution ambiguity)
//      * except in the case of interfaces
//      * or a single class with multiple interfaces
//      * or a single class with single inheritance and multiple interfaces
//  * all inheritances must be public virtual (avoids resolution ambiguity)
//  * all functions must be declared virtual 
//      * this further avoids resoltion ambiguity
//      * issue of multiple member copies still exist (same as Java)
//      * super class members and methods can be accessed using scope resolution


namespace c10m { namespace socket {

class InetProtocol {
private:

    static InetProtocol *singleton;
    static std::map<int, int> proto_map;

    InetProtocol() {
        proto_map.insert(std::pair<int, int>(IPPROTO_TCP, SOCK_STREAM));
    }

    static void create_instance(void) {

        if (singleton == nullptr) {
          singleton = new InetProtocol();
        }
    }

public:

    static int get_proto_mapping(int protocol) {
        create_instance();
        return proto_map.at(protocol);
    }
};

InetProtocol* InetProtocol::singleton = nullptr;
std::map<int, int> InetProtocol::proto_map;




InetServer::InetServer(const std::string& node, const std::string& service, 
        int protocol, std::vector<sockopt>& options, int backlog)
{
    int rc = -1;
    int server_sock = -1;
    struct addrinfo server_addr;
    struct addrinfo server_addr_hints;
    struct addrinfo *result_list = NULL;

    this->backlog_ = backlog;

    // tuple creation - hints to pass to getaddrinfo
    memset(&server_addr_hints, 0, sizeof(server_addr_hints));
    server_addr_hints.ai_family = AF_INET; // TODO: IPV6 support
    server_addr_hints.ai_socktype = InetProtocol::get_proto_mapping(protocol); 
    server_addr_hints.ai_flags = AI_PASSIVE; // For wildcard IP address
    server_addr_hints.ai_protocol = protocol;

    // tuple creation - create all possible tuples and fill dst_addr, dst_port
    rc = getaddrinfo(node.c_str(), service.c_str(), &server_addr_hints, &result_list);
    if (rc != 0) {
        throw io::IoException("inet-server-create: getaddrinfo:: %s\n", gai_strerror(rc));
    }

    // tuple creation - should be only one result based on the information passed
    if (result_list != NULL) {
        if (result_list->ai_next == NULL) {
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.ai_family = result_list->ai_family;
            server_addr.ai_socktype = result_list->ai_socktype;
            server_addr.ai_protocol = result_list->ai_protocol;
            server_addr.ai_addr = result_list->ai_addr;
            server_addr.ai_addrlen = result_list->ai_addrlen;
        }
        freeaddrinfo(result_list);
        if (result_list->ai_next != NULL) {
            throw io::IoException("inet-server-create: addr-list has more than one possible server address");
        }
    } else {
        throw io::IoException("inet-server-create: addr-list is null");
    }

    // tuple binding - `protocol` is binded to socket
    server_sock = ::socket(server_addr.ai_family, server_addr.ai_socktype, server_addr.ai_protocol);
    if (server_sock == -1) {
        throw io::IoException("server-create: socket:", errno);
    }
    this->set_fd(server_sock);

    // io options - various io options being set
    for (std::vector<sockopt>::iterator i = options.begin(); i != options.end(); ++i) {
        sockopt& option = *i;
        rc = setsockopt(server_sock, option.level, option.optname, option.optval, option.optlen);
        if (rc == -1) {
            this->close();
            throw io::IoException("server-create: setsockopt:", errno);
        }
    }

    // TODO: add this to the vector list somehow
    #if 0
    rc = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (rc == -1) {
        close(server_sock);
        throw io::IoException("server-create: setsockopt:", errno);
    }
    #endif

    // tuple binding - `dst_addr`, `dst_port` is binded socket
    rc = bind(server_sock, server_addr.ai_addr, server_addr.ai_addrlen);
    if (rc == -1) {
        this->close();
        throw io::IoException("server-create: bind:", errno);
    }

    std::cout << "server-create: socket " << server_sock << " ready on " << node << ":" << service << os::endl;
}

void InetServer::open(void)
{ 
    int ret = -1;
    int server_sock = this->get_fd();

    ret = listen(server_sock, this->backlog_);
    if (ret == -1) {
        throw io::IoException("server-open: listen:", errno);
    }

    std::cout << "server-open: socket " << server_sock << " listen with backlog " << this->backlog_ << os::endl;
}

void InetServer::close(void)
{
    int ret = -1;
    // TODO: reliable closing
    ret = ::close(this->get_fd());
    std::cout << "server-close: close:: return code " << ret << ", errno " << errno << os::endl;
}


int InetServer::peer_establish(void)
{
    return this; // TODO: fix
}

} }
