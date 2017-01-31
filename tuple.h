#ifndef C10M_NETIO__TUPLE_H_
#define C10M_NETIO__TUPLE_H_


#ifdef __cplusplus
namespace c10m_netio {
#endif

struct TupleClass {
    char *node;
    char *service;
    int (*create)(int *server_coket, const char *node, const char* service);
    int (*delete)(int server_coket);
};

enum TupleClassType {
    TUPLE_INET
};

#ifdef __cplusplus
extern "C" {
#endif

struct TupleClass tuple_inetsock;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif // C10M_NETIO__TUPLE_H_
