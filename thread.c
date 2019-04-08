#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>

typedef struct SockInfo {
    int fd;// 文件描述符
    struct sockaddr_in addr; // IP地址
    pthread_t id; // 线程ID
} SockInfo;

void *workerFunc(void *arg) {
    char ip[64];
    SockInfo *info = (SockInfo *) arg;
    // 通信
    while (1) {
        printf("client IP: %s Port:%d\n", inet_ntop(AF_INET, &info->addr.sin_addr.s_addr, ip, sizeof(ip)),
               ntohs(info->addr.sin_port));
        
        char buf[1024];

        int len = read(info->fd, buf, sizeof(buf));
        if (len == -1) {
            perror("read error");
            // 退出单个线程
            pthread_exit(NULL);
        } else if (len == 0) {
            printf("客户端断开连接\n");
            close(info->fd);
            break;
        } else {
            printf("recv buf: %s\n", buf);
            write(info->fd, buf, sizeof(buf));
        }
    }
    return NULL;
}

int main(int argc, const char *argv[]) {
    if (argc > 2) {
        printf("eg: ./a.out port\n");
        exit(1);
    }

    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    int port = atoi(argv[1]);

    // 创建套接字
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 初始化服务器 sockaddr_in
    memset(&server_addr, 0, server_len);
    server_addr.sin_family = AF_INET; // 地址族
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听本机所有的IP
    server_addr.sin_port = htons(port);// 设置端口

    // 绑定IP和端口
    bind(listen_fd, (struct sockaddr *) &server_addr, server_len);

    // 监听.设置同时监听的最大个数
    listen(listen_fd, 36);
    printf("start accept ... \n");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(struct sockaddr_in);

    int i = 0;
    SockInfo info[256];// 连接线程结构体数组，长度为256
    for (i = 0; i < sizeof(info) / sizeof(info[0]); ++i) {
        info[i].fd = -1;
    }
    while (1) {
        for (i = 0; i < 256; ++i) {
            if (info[i].fd == -1) {
                break;
            }
        }
        if (i == 256) break;

        // 主线程等待接收连接请求
        info[i].fd = accept(listen_fd, (struct sockaddr *) &info[i].addr, &client_len);

        // 创建子线程
        pthread_create(&info[i].id, NULL, workerFunc, &info[i]);
        // 设置线程分离，当进程关闭时不需要回收线程
        pthread_detach(info[i].id);
    }

    close(listen_fd);

    // 只退出主线程，子线程依旧执行
    pthread_exit(NULL);
}
