#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
// #include <bits/sigaction.h>
#include <sys/wait.h>
#include <errno.h>

void recyle(int num) {
    pid_t pid ;
    while (pid = waitpid(-1, NULL,WNOHANG) > 0)
    {
        printf("child died, pid = %d\n",pid);
    }
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

    // 使用信号来回收子进程pcb
    struct sigaction act;
    act.sa_handler = recyle;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGCHLD, &act, NULL);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        // 父进程接收连接请求
        // accept阻塞等待请求的时候如果被信号中断(回收子进程),处理完信号对应的操作之后
        // 回来之后不阻塞了，accept将会直接返回-1，这时候errno = EINTR
        int client_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &client_len);
        // if (client_fd == -1) {
        //     perror("accept error");
        //     exit(1);
        // }
        while (client_fd == -1 && errno == EINTR) {
            client_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &client_len);
        }

        // 创建子进程处理连接的请求
        pid_t pid = fork();
        // 子进程内，处理通信
        if (pid == 0) {
            close(listen_fd); // 关闭监听的fd，子进程类不会用到，如果不关闭就是浪费资源

            // 打印client的ip和port
            char ip[64];
            printf("connect successful\n");
            printf("client IP: %s, port: %d\n",
                   inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, ip, sizeof(ip)),
                   ntohs(client_addr.sin_port));
            while (1) {
                char buf[1024];
                int len = read(client_fd, buf, sizeof(buf));
                if (len == -1) {
                    perror("read error");
                    exit(1);
                }
                else if (len == 0) {
                    printf("客户端断开了连接\n");
                    close(client_fd);
                    break;
                }
                else {
                    printf("recv buf: %s\n", buf);
                    write(client_fd, buf, len);
                }
            }
            // 关闭子进程
            return ;
        }
        // 父进程
        else {
            // 关闭客户端fd，父进程进程类不会用到，如果不关闭就是浪费资源
            close(client_fd);
        }
    }

    close(listen_fd);
    return 0;
}