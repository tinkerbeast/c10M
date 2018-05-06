#ifndef C10M_NETIO__NET_IO_INTERFACE_H_
#define C10M_NETIO__NET_IO_INTERFACE_H_

// *** macros ******************************************************************

#ifdef __cplusplus
namespace c10m_netio {
#endif
    
// *** typedefs and enums ******************************************************

// *** user-define PODs ********************************************************

// *** classes *****************************************************************
#ifdef __cplusplus

class NetIoInterface: {
    public:
        virtual int get_fd() = 0;
        virtual int open_fd(const char *node, const char *service, const char *proto, int flags) = 0;
        virtual int close_fd();
        virtual int read_all(void *buffer, size_t in_size, size_t out_size) = 0;
        virtual int write_all(void *buffer, size_t in_size, size_t out_size) = 0;
        virtual int read_available(void *buffer, size_t in_size, size_t out_size) = 0;
        virtual int write_available(void *buffer, size_t in_size, size_t out_size) = 0;
};

#endif

// *** prototypes and variables*************************************************
#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif

// *** static-inline functions *************************************************

#ifdef __cplusplus
}
#endif // c10m_netio

#endif // C10M_NETIO__NET_IO_INTERFACE_H_
