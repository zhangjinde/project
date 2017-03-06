
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h> //文件属性修改
#include <netinet/tcp.h>

#include "../include/anet.h"

int anetSetError(char *err, const char *fmt, ...) {
    if (!fmt || err == NULL) {
        return ANET_ERR;
    }

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, ANET_ERR_LEN, fmt, ap);
    va_end(ap);

    return ANET_OK;
}

/**
 * 设置地址重用setsockopt SO_REUSEADDR
 * @param  err 错误描述
 * @param  fd  文件描述符
 *
 *
 * @return     int
 */
static int anetSetReuseAddr(char *err, int fd) {
    int yes = 1;
    // printf("%d %d %lu\n", fd, yes, sizeof(yes));
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt SO_REUSEADDR failed :%s", strerror(errno));
        return ANET_ERR;
    }

    return ANET_OK;
}

static int anetCreateSocket(char *err, int domain) {
    int sock = socket(domain, SOCK_STREAM, 0);
    if (sock < 0) {
        anetSetError(err, "Create socket failed :%s", strerror(errno));
        return ANET_ERR;
    }

    //设置重用选项，可以多次close/open同一个socket
    if (ANET_ERR == anetSetReuseAddr(err, sock)) {
        close(sock);
        return ANET_ERR;
    }
    return sock;
}

static int anetListen(char *err, int s, struct sockaddr *addr, socklen_t len, int backlog) {
    if (bind(s, addr, len) == -1) {
        close(s);
        anetSetError(err, "bind failed:%s", strerror(errno));
        return ANET_ERR;
    }

    if (listen(s, backlog)) {
        close(s);
        anetSetError(err, "listen failed", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

static int _anetTcpServer(char *err, int port, char *bindaddr, int af, int backlog) {
    char _port[6];
    int rv, sock;
    struct addrinfo hints, *serverinfo, *p;

    snprintf(_port, 6, "%d", port);
    memset(&hints, '\0', sizeof(hints));
    hints.ai_family = af;       //协议族
    hints.ai_socktype = SOCK_STREAM; //TCP
    hints.ai_flags = AI_PASSIVE; //是否将取得的socket地址用于被动打开。

    if ((rv = getaddrinfo(bindaddr, _port, &hints, &serverinfo)) != 0) {
        anetSetError(err, "getaddrinfo failed %s", gai_strerror(rv));
        return ANET_ERR;
    }

    for (p = serverinfo; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }

        //TODO IPV6 判断

        if (anetSetReuseAddr(err, sock) == ANET_ERR) {
            return ANET_ERR;
        }

        if (anetListen(err, sock, p->ai_addr, p->ai_addrlen, backlog) == ANET_ERR ) {
            return ANET_ERR;
        }
        break;
    }
    if (p == NULL) {
        anetSetError(err, "Unable to bind socket");
        return ANET_ERR;
    }

    if (serverinfo) {
        freeaddrinfo(serverinfo);
    }
    return sock;
}

static int anetGenericAccept(char *err, int sock, struct sockaddr *sa, socklen_t *len) {
    int fd;
    while (1) {
        if ((fd = accept(sock, sa, len)) == -1) {
            if (errno == EINTR) { //阻塞操作过程中，中断信号，错误返回
                continue;
            } else {
                anetSetError(err, "accept error :%s", strerror(errno));
                return ANET_ERR;
            }
        }
        break;
    }
    return fd;
}

int anetTcpServer(char *err, int port, char *bindaddr, int backlog) {
    return _anetTcpServer(err, port, bindaddr, AF_INET, backlog);
}

int anetTcp6Server(char *err, int port, char *bindaddr, int backlog) {
    return _anetTcpServer(err, port, bindaddr, AF_INET6, backlog);
}

/**
 * 接收TCP socket连接，使用通用的struct sockaddr_storage结构接收，用以兼容IPV4, IPV6
 *
 * @param  err        错误提示
 * @param  serversock socket fd
 * @param  ip         output 返回的ip地址
 * @param  ip_len     input ip地址空间
 * @param  port       output 返回的端口号
 * @return            是否正常返回标识
 */
int anetTcpAccept(char *err, int serversock, char *ip, size_t ip_len, int *port) {
    int fd;

    // struct sockaddr sa;
    struct sockaddr_storage sa;
    socklen_t len;
    if ((fd = anetGenericAccept(err, serversock, (struct sockaddr*)&sa, &len)) == ANET_ERR) {
        close(serversock);
        return ANET_ERR;
    }

    if (sa.ss_family == AF_INET) { //ipv4
        struct sockaddr_in *si = (struct sockaddr_in *)&sa;
        if (ip) {
            inet_ntop(AF_INET, &si->sin_addr, ip, ip_len);
        }
        if (port) {
            *port = ntohs(si->sin_port);
        }
    } else {//ipv6
        struct sockaddr_in6 *si = (struct sockaddr_in6 *)&sa;
        if (ip) {
            inet_ntop(AF_INET6, &si->sin6_addr, ip, ip_len);
        }
        if (port) {
            *port = ntohs(si->sin6_port);
        }
    }

    return ANET_OK;
}

/**
 * 是否开启socket的阻塞或非阻塞操作
 * TODO 设置非阻塞再置为阻塞时，accept获取的ip不对
 * @param  err   err
 * @param  fd    fd
 * @param  block 1阻塞 0 非阻塞
 * @return
 */
static int anetSetSocketBlock(char *err, int fd, int block) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        anetSetError(err, "fcntl getfl failed: %s", strerror(errno));
        return ANET_ERR;
    }

    if (block) {
        flags &= ~O_NONBLOCK; //要使用按位与
    } else {
        flags |= O_NONBLOCK;  //按位或
    }

    if (fcntl(fd, F_SETFL, flags) == -1) {
        anetSetError(err, "fcntl F_SETFL O_NONBLOCK failed %s", strerror(errno));
        return ANET_ERR;
    }

    return ANET_OK;
}

int anetBlock(char *err, int fd) {
    return anetSetSocketBlock(err, fd, 1);
}

int anetNonBlock(char *err, int fd) {
    return anetSetSocketBlock(err, fd, 0);
}

/**
 * 是否关闭Nagles算法
 * @param  err     错误描述
 * @param  fd      文件描述符
 * @param  nodelay 1关闭，0开启
 * @return
 */
static int anetSetTcpNoDelay(char *err, int fd, int nodelay) {
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) == -1) {
        anetSetError(err, "setsockopt TCP_NODELAY failed :%s", strerror(errno));
        return ANET_ERR;
    }

    return ANET_OK;
}

int anetEnableTcpNoDelay(char *err, int fd) {
    return anetSetTcpNoDelay(err, fd, 1);
}

int anetDisableTcpNoDelay(char *err, int fd) {
    return anetSetTcpNoDelay(err, fd, 0);
}

int anetTcpKeepAlive(char *err, int fd) {
    int keepalive = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) == -1) {
        anetSetError(err, "setsockopt SO_KEEPALIVE failed :%s", strerror(errno));
        return ANET_ERR;
    }

    return ANET_OK;
}

int anetSendTimeout(char *err, int fd, long long ms) {
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms - tv.tv_sec*1000)*1000;

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1) {
        anetSetError(err, "setsockopt SO_SNDTIMEO failed :%s", strerror(errno));
        return ANET_ERR;
    }

    return ANET_OK;
}

#define ANET_CONNECT_NONE 0         //连接
#define ANET_CONNECT_NONBLOCK 1     //非阻塞连接
#define ANET_CONNECT_BE_BINDING 2   //最大努力binding
/**
 * TCP发起socket连接
 * TODO bind 端口号是啥意思
 * @param  err         错误描述
 * @param  addr        地址
 * @param  port        端口号
 * @param  source_addr 绑定地址号
 * @param  flags       标志位
 * @return             [description]
 */
static int anetTcpGenericConnect(char *err, char *addr, int port, char *source_addr, int flags) {
    char _port[6];
    int rv, sock = ANET_ERR;
    struct addrinfo hints, *serverinfo, *p, *bserverinfo, *b;

    snprintf(_port, 6, "%d", port);
    memset(&hints, '\0', sizeof(hints));
    hints.ai_family = AF_UNSPEC;     //AF_UNSPEC则意味着函数返回的是适用于指定主机名和服务名且适合任何协议族的地址。
    hints.ai_socktype = SOCK_STREAM; //TCP

    if ((rv = getaddrinfo(addr, _port, &hints, &serverinfo)) != 0) {
        anetSetError(err, "getaddrinfo failed %s", gai_strerror(rv));
        return ANET_ERR;
    }

    for (p = serverinfo; p != NULL; p = p->ai_next) {
        //尝试创建一个socket连接并连接，如果失败，尝试下一个serverino
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }

        if (anetSetReuseAddr(err, sock) == ANET_ERR) {
            goto error;
        }

        if (flags & ANET_CONNECT_NONBLOCK && anetNonBlock(err, sock) != ANET_OK) {
            goto error;
        }

        //TODO 为啥绑地址
        if (source_addr) {
            int bound = 0;
            if ((rv = getaddrinfo(source_addr, NULL, &hints, &bserverinfo)) != 0) {
                anetSetError(err, "getaddrinfo failed %s", gai_strerror(rv));
                return ANET_ERR;
            }

            for (b = bserverinfo; b != NULL; b = b->ai_next) {
                if (bind(sock, b->ai_addr, b->ai_addrlen) != -1) {
                    bound = 1;
                    break;
                }
            }

            freeaddrinfo(bserverinfo);
            if (!bound) {
                anetSetError(err, "bind: %s", strerror(errno));
                goto error;
            }
        }

        if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
            //非阻塞连接，直接goto end
            if (errno == EINPROGRESS && flags & ANET_CONNECT_NONBLOCK)
                goto end;
            close(sock);
            sock = ANET_ERR;
            continue;
        }
        goto end;
    }
    if (p == NULL) {
        anetSetError(err, "creating socket: %s", strerror(errno));
        return ANET_ERR;
    }
error:
    if (sock != ANET_ERR) {
        close(sock);
        return ANET_ERR;
    }
end:
    if (serverinfo) {
        freeaddrinfo(serverinfo);
    }

    //如果设置了最大努力bind,但是失败了，则尝试不设置source_addr,重连
    if (sock == ANET_ERR && source_addr && (flags & ANET_CONNECT_BE_BINDING)) {
        return anetTcpGenericConnect(err, addr, port, NULL, flags);
    } else {
        return sock;
    }
}

int anetTcpConnect(char *err, char *addr, int port) {
    return anetTcpGenericConnect(err, addr, port, NULL, ANET_CONNECT_NONE);
}
int anetTcpNonblockConnect(char *err, char *addr, int port) {
    return anetTcpGenericConnect(err, addr, port, NULL, ANET_CONNECT_NONBLOCK);
}
int anetTcpNonblockBindConnect(char *err, char *addr, int port, char *source_addr) {
    return anetTcpGenericConnect(err, addr, port, source_addr, ANET_CONNECT_NONBLOCK);
}
int anetTcpNonBlockBestEffortBindConnect(char *err, char *addr, int port, char *source_addr) {
    return anetTcpGenericConnect(err, addr, port, source_addr, ANET_CONNECT_NONBLOCK|ANET_CONNECT_BE_BINDING);
}

/**
 * 读取count个字符到buf，直到发生错误或EOF
 * @param  fd    fd
 * @param  buf   读取缓存
 * @param  count 要读取的字节数
 * @return       读取的字节数
 */
int anetRead(int fd, char *buf, int count) {
    int nread, totlen = 0;
    while (totlen < count) {
        nread = read(fd, buf, count-totlen);
        if (nread == 0) {// 读取完成
            return totlen;
        }
        if (nread == -1) {
            return -1;
        }
        totlen += nread;
        buf += nread;
    }
    return totlen;
}

/**
 * 向fd中写count个字符，直到发生错误
 * @param  fd
 * @param  buf   要写入的字符串
 * @param  count 要写入的字符个数
 * @return       成功写入的字符数
 */
int anetWrite(int fd, char *buf, int count) {
    int nwrite, totlen = 0;
    while (totlen < count) {
        nwrite = write(fd, buf, count);
        if (nwrite == 0) {
            return totlen;
        }
        if (nwrite == -1) {
            return -1;
        }

        totlen += nwrite;
        buf += nwrite;
    }
    return totlen;
}

/**
 * 获取socket对端的ip地址和端口号，
 * @param  fd     [description]
 * @param  ip     [description]
 * @param  ip_len [description]
 * @param  port   [description]
 * @return        [description]
 */
int anetPeerToString(int fd, char *ip, size_t ip_len, int *port) {
    struct sockaddr_storage sa;
    socklen_t len;

    if ((getpeername(fd, (struct sockaddr *)&sa, &len)) == ANET_ERR) {
        goto error;
    }

    if (sa.ss_family == AF_INET) { //ipv4
        struct sockaddr_in *si = (struct sockaddr_in *)&sa;
        if (ip) {
            inet_ntop(AF_INET, &si->sin_addr, ip, ip_len);
        }
        if (port) {
            *port = ntohs(si->sin_port);
        }
    } else if (sa.ss_family == AF_INET6) {//ipv6
        struct sockaddr_in6 *si = (struct sockaddr_in6 *)&sa;
        if (ip) {
            inet_ntop(AF_INET6, &si->sin6_addr, ip, ip_len);
        }
        if (port) {
            *port = ntohs(si->sin6_port);
        }
    } else if (sa.ss_family == AF_UNIX) {
        if (ip) strncpy(ip,"/unixsocket",ip_len);
        if (port) *port = 0;
    } else {
        goto error;
    }

    return ANET_OK;
error:
    if (ip) {
        if (ip_len >= 2) {
            ip[0] = '?';
            ip[1] = '\0';
        } else if (ip_len == 1) {
            ip[0] = '\0';
        }
    }
    if (port) *port = 0;

    return ANET_ERR;
}

/**
 * 获取本端的IP地址和端口号
 * @param  fd     [description]
 * @param  ip     [description]
 * @param  ip_len [description]
 * @param  port   [description]
 * @return        [description]
 */
int anetSockname(int fd, char *ip, size_t ip_len, int *port) {
    struct sockaddr_storage sa;
    socklen_t len;

    if ((getsockname(fd, (struct sockaddr *)&sa, &len)) == ANET_ERR) {
        if (port) *port = 0;
        if (ip) {
            ip[0] = '?';
            ip[1] = '\0';
        }
        return ANET_ERR;
    }

    if (sa.ss_family == AF_INET) { //ipv4
        struct sockaddr_in *si = (struct sockaddr_in *)&sa;
        if (ip) {
            inet_ntop(AF_INET, &si->sin_addr, ip, ip_len);
        }
        if (port) {
            *port = ntohs(si->sin_port);
        }
    } else {//ipv6
        struct sockaddr_in6 *si = (struct sockaddr_in6 *)&sa;
        if (ip) {
            inet_ntop(AF_INET6, &si->sin6_addr, ip, ip_len);
        }
        if (port) {
            *port = ntohs(si->sin6_port);
        }
    }
    return ANET_OK;
}