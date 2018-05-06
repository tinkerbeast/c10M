#ifndef C10M_SOCKET_H_
#define C10M_SOCKET_H_

#include "c10m_cpp03compat.h"
#include "c10m_io.h"
#include <string>
#include <vector>

// *** macros ******************************************************************

#ifdef __cplusplus
namespace c10m {
namespace socket {
#endif
    
// *** typedefs ****************************************************************

// *** user-define PODs ********************************************************

// *** classes *****************************************************************


struct sockopt {
    int level;
    int optname;
    void *optval;
    socklen_t optlen;
};

class SocketInterface : virtual public io::IoInterface {
public:
    virtual io::IoInterface* peer_establish(void) = 0;
    virtual ~SocketInterface() { }
};


class InetServer : virtual public SocketInterface, virtual public io::IoAbstract {
private:
    int backlog_;
public:
    InetServer(const std::string& node, const std::string& service, 
            int protocol, std::vector<sockopt>& options, int backlog);
    virtual void open(void);
    virtual void close(void);
    virtual int peer_establish(void);
};


// *** prototypes and variables*************************************************
#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
}
#endif

#endif // C10M_NETIO__TUPLE_H_
