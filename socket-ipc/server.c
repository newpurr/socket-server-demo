#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>

int main(int argc, const char *argv[]) {
    // 创建套接字
    int listen_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket error");
        exit(1);
    }

    // 绑定监听的本地套接字
    unlink("server.sock");
    struct sockaddr_un server;
    server.sun_family = AF_LOCAL;
    strcpy(server.sun_path, "server.sock");
    int ret = bind(listen_fd, (struct sockaddr *) &server, sizeof(server));
    if (ret == -1) {
        perror("bind error");
        exit(1);
    }

    // 监听
    ret = listen(listen_fd, 36);
    if (ret == -1) {
        perror("listem error");
        exit(1);
    }

    // 等待接收连接请求
    struct sockaddr_un client;
    socklen_t socklen = sizeof(client);
    int client_fd = accept(listen_fd, (struct sockaddr *) &client, &socklen);
    if (client_fd == -1) {
        perror("accept error");
        exit(1);
    }
    
    printf("client bind file:%s \n", client.sun_path);

    // 通信
    while (1) {
        char buf[1024] = {0};
        int recvlen = recv(client_fd, buf, sizeof(buf), 0);
        if (recvlen == -1) {
            perror("recv error");
            exit(1);
        } else if (recvlen == 0) {
            printf("client disconnect ... \n");
            close(client_fd);
            break;
        } else {
            printf("recv buf:%s \n", buf);
            send(client_fd, buf, sizeof(buf), 0);
        }
    }
    close(listen_fd);
    close(client_fd);
    return 0;
}
