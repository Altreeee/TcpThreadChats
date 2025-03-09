#include <map>
#include "header.h"
using std::map;

/*
服务端使用 std::map 记录所有已连接的客户端套接字及其地址信息。
每个客户端连接后，服务端会为其创建一个线程，用于处理与该客户端的通信。
服务端可以接收来自客户端的消息，并将消息群发给所有已连接的客户端。
服务端支持动态管理客户端连接，当客户端断开时会释放资源。
*/

/*
socks 是一个 std::map，用于记录所有已连接的客户端。
键：客户端的套接字文件描述符（int 类型）。
值：指向客户端地址信息的指针（struct sockaddr_in*）。
这个数据结构的作用：
记录所有已连接的客户端。
方便实现群发功能（遍历所有客户端套接字）。
*/
map<int, struct sockaddr_in*> socks;         // 用于记录各个客户端，键是与客户端通信 socket 的文件描述符，值是对应的客户端的 sockaddr_in 的信息

// 群发消息给 socks 中的所有客户端
/*
参数：
    buf：要发送的消息。
    len：消息的长度。
实现：
    遍历 socks 中的所有客户端套接字。
    使用 send 函数将消息发送到每个客户端。
*/
inline void send_all(const char *buf, int len)
{
    for(auto it = socks.begin(); it != socks.end(); ++it)
        send(it->first, buf, len, 0);
}

// 服务端端接收消息的线程函数
/*
功能：
    接收来自某个客户端的消息，并将其群发给所有客户端。
参数：
    args：传递的参数是客户端套接字文件描述符的地址。
逻辑：
    从 args 中获取客户端的套接字文件描述符 cfd。
    循环接收客户端发送的消息：
        使用 recv 函数接收数据。
        如果 recv 返回值 n <= 0，表示客户端断开连接或发生错误，退出循环。
        将接收到的消息打印到服务端的标准输出。
        如果接收到消息是 "bye\n"，表示客户端请求断开连接：
            打印断开信息。
            从 socks 中删除该客户端，并释放动态分配的内存。
            退出循环。
        将消息群发给所有客户端。
    关闭客户端的套接字。
*/
void* recv_func(void* args)
{
    int cfd = *(int*)args;
    char buf[BUF_LEN];
    while(true) {
        int n = recv(cfd, buf, BUF_LEN, 0);
        if(n <= 0)   break;                     // 关键的一句，用于作为结束通信的判断
        write(STDOUT_FILENO, buf, n);
//        printf("buf = %s  len(buf) = %ld\n", buf, strlen(buf));
        if(strcmp(buf, "bye\n") == 0) {         // 如果接收到客户端的 bye，就结束通信并从 socks 中删除相应的文件描述符，动态申请的空间也应在删除前释放
            printf("close connection with client %d.\n", cfd);
            free(socks[cfd]);
            socks.erase(cfd);
            break;
        }
        send_all(buf, n);           // 群发消息给所有已连接的客户端
    }
    close(cfd);                 // 关闭与这个客户端通信的文件描述符
}

// 和某一个客户端通信的线程函数
/*
功能：
    处理与某个客户端的通信，包括接收和发送消息。
逻辑：
    创建一个线程运行 recv_func，用于接收该客户端的消息。
    主线程负责读取服务端的标准输入（STDIN_FILENO），将服务端输入的消息群发给所有客户端。
    如果服务端输入结束，关闭客户端的套接字。
*/
void* process(void *argv)
{
    pthread_t td;
    pthread_create(&td, NULL, recv_func, (void*)argv);         // 在主处理函数中再新开一个线程用于接收该客户端的消息

    int sc = *(int*)argv;
    char buf[BUF_LEN];
    while(true) {
        int n = read(STDIN_FILENO, buf, BUF_LEN);
        buf[n++] = '\0';                // 和客户端一样需要自己手动添加字符串结束符
        send_all(buf, n);               // 服务端自己的信息输入需要发给所有客户端
    }
    close(sc);
}

/*
功能：
    服务端的主逻辑，负责监听客户端连接并为每个客户端创建线程。
逻辑：
    初始化服务端地址结构 serv，绑定到指定端口。
    创建套接字 ss，并绑定到服务端地址。
    调用 listen 函数进入监听模式，等待客户端连接。
    循环处理客户端连接：
        使用 accept 接受客户端连接。
        如果连接数超过最大限制（MAX_CONNECTION），发送提示消息并关闭连接。
        将客户端的套接字和地址信息存储到 socks 中。
        为每个客户端创建一个线程运行 process 函数，处理与该客户端的通信。
*/
/*
代码运行流程
    1，服务端启动后，进入监听模式，等待客户端连接。
    2，每当有客户端连接：
        服务端接受连接，并将客户端信息存储到 socks 中。
        为该客户端创建一个线程，处理与其通信。
    3，客户端发送消息：
        服务端接收消息，并将其群发给所有已连接的客户端。
    4，客户端断开连接：
        服务端检测到连接断开，释放资源并从 socks 中删除该客户端。
    5，服务端可以通过标准输入发送消息，群发给所有客户端。
*/
int main(int argc, char *argv[])
{
    struct sockaddr_in serv;
    bzero(&serv, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port = htons(PORT);

    int ss = socket(AF_INET, SOCK_STREAM, 0);
    if(ss < 0) {
        perror("socket error");
        return 1;
    }
    int err = bind(ss, (struct sockaddr*)&serv, sizeof(serv));
    if(err < 0) {
        perror("bind error");
        return 2;
    }
    err = listen(ss, 2);
    if(err < 0) {
        perror("listen error");
        return 3;
    }

    socks.clear();          // 清空 map
    socklen_t len = sizeof(struct sockaddr);

    while(true) {
        struct sockaddr_in *cli_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        int sc = accept(ss, (struct sockaddr*)cli_addr, &len);
        if(sc < 0) {
            free(cli_addr);
            continue;
        }
        if(socks.size() >= MAX_CONNECTION) {            // 当将要超过最大连接数时，就让那个客户端先等一下
            char buf[128] = "connections is too much, please waiting...\n";
            send(sc, buf, strlen(buf) + 1, 0);
            close(sc);
            free(cli_addr);
            continue;
        }
        socks[sc] = cli_addr;                        // 指向对应申请到的 sockaddr_in 空间
        printf("client %d connect me...\n", sc);

        pthread_t td;
        pthread_create(&td, NULL, process, (void*)&sc);       // 开一个线程来和 accept 的客户端进行交互
    }
    return 0;
}
