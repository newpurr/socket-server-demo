#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#define SERV_PORT   6608
#define MAX_EVENTS  1024
#define BUFLEN      4096

void recvdata(int fd, int events, void *arg);

void senddata(int fd, int events, void *arg);

int g_efd;                                                // 全局变量，保存epoll_create返回的文件描述符

struct myevent_s {
    int fd;                                               // 监听的文件描述符
    int events;                                           // 监听的事件
    void *arg;                                            // 泛型参数
    void (*call_back)(int fd, int events, void *arg);     // 回调函数指针
    int status;                                          // 状态
    char buf[BUFLEN];                                     // 缓冲区
    int len;                                             // buf存放的有效字节数
    long last_active;                                     // 最后操作文件描述符的事件
};

struct myevent_s g_events[MAX_EVENTS + 1];

/*void eventSet(struct myevent_s *ev, int fd, void (*call_back)(int, int, void *), void *arg) {


    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    bind(lfd, (struct sockaddr *) &sin, sizeof(sin));

    listen(lfd, 20);
    return;
}*/

void eventAdd(int efd, int events, struct myevent_s *ev) {
    struct epoll_event epv;

    int op;
    epv.events = ev->events = events;// EPOLLIN 或者 EPOLLOUT
    epv.data.ptr = ev;

    if (ev->status == 1) {
        op = EPOLL_CTL_MOD;
    } else {
        op = EPOLL_CTL_ADD;
        ev->status = 1;
    }

    if (epoll_ctl(efd, op, ev->fd, &epv) < 0) {
        printf("event add failed [fd=%d], events[%d]\n", ev->fd, events);
    } else {
        printf("event add OK [fd=%d],[op=%d], events[%0X]\n", ev->fd, op, events);
    }
}

void eventSet(struct myevent_s *ev, int fd, void (*call_back)(int, int, void *), void *arg) {
    ev->fd = fd;
    ev->call_back = call_back;
    ev->status = 0;
    ev->arg = arg;
    ev->status = 0;
    ev->last_active = time(NULL);
}

void eventDel(int efd, struct myevent_s *ev) {
    struct epoll_event epv;

    if (ev->status != 1) {
        return;
    }

    epv.data.ptr = ev;
    ev->status = 0;
    epoll_ctl(efd, EPOLL_CTL_DEL, ev->fd, &epv);
}

void acceptConn(int lfd, int events, void *arg) {
    struct sockaddr_in cin;
    socklen_t len = sizeof(cin);
    int cfd, i;

    if ((cfd = accept(lfd, (struct sockaddr *) &cin, &len)) == -1) {
        if (errno != EAGAIN && errno != EINTR) {
            /* 暂时不做处理 */
        }
        printf("%s:accept, %s\n", __func__, strerror(errno));
        return;
    }

    do {
        // 从全局数组中找出一个空事件成员
        for (i = 0; i < MAX_EVENTS; i++) {
            if (g_events[i].status == 0) {
                break;
            }
        }

        if (i == MAX_EVENTS) {
            printf("%s: max connect limit [%d]\n", __func__, MAX_EVENTS);
            break;// 跳出do=while循，不执行后续代码
        }

        // 设置cfd为非阻塞
        if ((fcntl(cfd, F_SETFL, O_NONBLOCK)) < 0) {
            printf("%s: fcntl o_nonblock failed %s \n", __func__, strerror(errno));
            break;
        }

        // 给cfd设置一个myevent_s结构体，回调函数设置为recvdata
        eventSet(&g_events[i], cfd, recvdata, &g_events[i]);

        // 将cfd添加到epoll红黑树上，监听IN事件
        eventAdd(g_efd, EPOLLIN, &g_events[i]);

    } while (0);

    printf("new connect [%s:%d][time:%ld],pos[%d]\n", inet_ntoa(cin.sin_addr),
           ntohs(cin.sin_port), g_events[i].last_active, i);
}

void initListenSocket(int efd, short port) {
    int lfd = socket(AF_INET, SOCK_STREAM, 0);

    // 设置非阻塞
    fcntl(lfd, F_SETFL, O_NONBLOCK);

    /* void eventSet(struct myevent_s *ev, int fd, void (*call_back) (int, int, void *), void *arg); */
    eventSet(&g_events[MAX_EVENTS], lfd, acceptConn, &g_events[MAX_EVENTS]);

    /* void eventAdd(int efd, int events, struct myevent_s *ev); */
    eventAdd(efd, EPOLLIN, &g_events[MAX_EVENTS]);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    bind(lfd, (struct sockaddr *) &sin, sizeof(sin));

    listen(lfd, 20);
}

void recvdata(int fd, int events, void *arg)
{
    struct myevent_s *ev = (struct myevent_s *) arg;
    int len;

    // 读取数据，放入ev的buf中
    len = recv(fd, ev->buf, sizeof(ev->buf), 0);

    // 从epoll中删除监听
    eventDel(g_efd, ev);

    if (len > 0) {
        ev->len = len;
        ev->buf[len] = "\0";
        printf("C[%d]:%s\n", fd, ev->buf);

        eventSet(ev, fd, senddata, ev);

        eventAdd(g_efd, EPOLLOUT, ev);
    } else if (len == 0) {
        close(ev->fd);
        /* ev-g_events 地址相减得到偏移元素位置 */
        printf("[fd=%d] pos[%ld], close\n", fd, ev - g_events);
    } else {

    }
}

void senddata(int fd, int events, void *arg)
{
    printf("this is send data\n");
}

int main(int argc, char *argv[]) {
    unsigned short port = SERV_PORT;

    if (argc == 2) {
        port = atoi(argv[1]);
    }

    g_efd = epoll_create(MAX_EVENTS + 1);
    if (g_efd <= 0) {
        printf("create efd in %s error %s\n", __func__, strerror(errno));
        exit(1);
    }

    initListenSocket(g_efd, port);

    struct epoll_event events[MAX_EVENTS + 1];

    printf("server running:port[%d]\n", port);

    int checkpos = 0, i;
    while (1) {
        /* 超时验证，每次检测100个连接，不测试listenfd 当客户端60秒内没有和服务器通信，则关闭客户端连接 */
        long now = time(NULL);
        for (i = 0; i < 100; i++, checkpos++) {

            if (checkpos == MAX_EVENTS) {
                checkpos = 0;
            }

            if (g_events[checkpos].status != 1) {                  // 不在红黑树g_efd上
                continue;
            }

            long duration = now - g_events[checkpos].last_active;

            if (duration >= 80) {
                close(g_events[checkpos].fd);                       // 关闭连接
                printf("[fd=%d] timeout\n", g_events[checkpos].fd);
                eventDel(g_efd, &g_events[checkpos]);               // 将该客户端从红黑树上移除
            }
        }

        /* 阻塞等待事件发生 */
        int nfd = epoll_wait(g_efd, events, MAX_EVENTS + 1, -1);
        if (nfd <= 0) {
            printf("epoll_wait error, exit \n");
            break;
        }

        for (i = 0; i < nfd; i++) {
            struct myevent_s *ev = (struct myevent_s *) events[i].data.ptr;

            if ((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)) {
                ev->call_back(ev->fd, events[i].events, ev->arg);
            }

            if ((events[i].events) & EPOLLOUT && (ev->events & EPOLLOUT)) {
                ev->call_back(ev->fd, events[i].events, ev->arg);
            }
        }
    }

    return 0;
}
