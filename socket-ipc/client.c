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
    int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket error");
        exit(1);
    }

    unlink("client.sock");
    // 连接服务器
    // 1.给客户端绑定套接字
    struct sockaddr_un client;
    client.sun_family = AF_LOCAL;
    strcpy(client.sun_path, "client.sock");
    int ret = bind(fd, (struct sockaddr*) &client, sizeof(client));
    if (ret == -1) {
        perror("bind error");
        exit(1);
    }

    // 2. 连接服务器
    struct sockaddr_un server;
    server.sun_family = AF_LOCAL;
    strcpy(server.sun_path, "server.sock");
    connect(fd, (struct sockaddr*)&server, sizeof(server));

    // 3.通信
    while (1) {
        char buf[1024] = {0};
        fgets(buf, sizeof(buf), stdin);
        printf("recv stdin buf:%s \n", buf);
        send(fd, buf, sizeof(buf), 0);
        int recvlen = recv(fd, buf, sizeof(buf), 0);
        if (recvlen == -1) {
            perror("recv error");
            exit(1);
        } else if (recvlen == 0) {
            printf("client disconnect ... \n");
            break;
        } else {
            printf("recv buf:%s \n", buf);
        }
    }

    close(fd);
    return 0;
}
