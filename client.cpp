#include "header.h"

// 客户端接收消息的线程函数
/*
功能：
    这是一个线程函数，用于接收服务器发送的消息并打印到终端。
参数：
    args 是一个指向套接字文件描述符的指针。
逻辑：
    创建一个缓冲区 buf，用于存储接收到的数据。
    使用 recv 函数从服务器接收数据：
    recv(sock_fd, buf, BUF_LEN, 0)：从套接字 sock_fd 接收最多 BUF_LEN 字节的数据。
    如果返回值 n <= 0，表示连接已关闭或发生错误，退出循环。
    使用 write(STDOUT_FILENO, buf, n) 将接收到的数据直接写到标准输出（终端）。
    关闭套接字并退出线程。
*/
void* recv_func(void *args)
{
    char buf[BUF_LEN];
    int sock_fd = *(int*)args;
    while(true) {
        int n = recv(sock_fd, buf, BUF_LEN, 0);
        if(n <= 0)   break;                  // 这句很关键，一开始不知道可以用这个来判断通信是否结束，用了其它一些很奇葩的做法来结束并关闭 sock_fd 以避免 CLOSE_WAIT 和 FIN_WAIT2 状态的出现T.T
        write(STDOUT_FILENO, buf, n);
    }
    close(sock_fd);
    exit(0);
}

// 客户端和服务端进行通信的处理函数
/*
功能：
    处理客户端与服务器的通信，包括发送消息和接收消息。
逻辑：
    创建一个新线程：
        使用 pthread_create 启动一个线程运行 recv_func，用于接收服务器的消息。
        这样可以避免传统的“一读一写”模式，允许客户端同时发送和接收消息。
    主线程进入循环，等待用户输入：
        使用 read(STDIN_FILENO, buf, BUF_LEN) 从标准输入读取用户输入。
        手动在缓冲区末尾添加字符串结束符 \0，因为标准输入不会自动添加。
        使用 send(sock_fd, buf, n, 0) 将用户输入的数据发送给服务器。
    如果用户输入结束或发生错误，关闭套接字并退出。
*/
void process(int sock_fd)
{
    pthread_t td;
    pthread_create(&td, NULL, recv_func, (void*)&sock_fd);      // 新开个线程来接收消息，避免了一读一写的原始模式，一开始竟把它放进 while 循环里面了，泪崩。。。

    char buf[BUF_LEN];
    while(true) {
        int n = read(STDIN_FILENO, buf, BUF_LEN);
        buf[n++] = '\0';                            // 貌似标准读入不会有字符串结束符的，需要自己手动添加
        send(sock_fd, buf, n, 0); 
        //我：send 函数是 POSIX 标准的一部分，定义在 <sys/socket.h> 头文件中。它是用于通过套接字发送数据的函数，通常用于网络编程。
    }
    close(sock_fd);
}


/*
功能：
    程序的入口，负责初始化客户端并连接到服务器。
逻辑：
    命令行参数检查：
        使用 assert(argc == 2) 确保命令行参数数量正确。
        程序需要一个命令行参数（服务器的 IP 地址）。
    初始化客户端地址结构：
        struct sockaddr_in cli：定义一个 IPv4 地址结构。
        bzero(&cli, sizeof(cli))：将结构体清零。
        cli.sin_family = AF_INET：指定地址族为 IPv4。
        cli.sin_addr.s_addr = htonl(INADDR_ANY)：设置 IP 地址为任意地址（这里会被后续的 inet_pton 覆盖）。
        cli.sin_port = htons(PORT)：将端口号转换为网络字节序。
        如果不使用 htons，可能会因为主机字节序和网络字节序的不同导致连接失败。
    创建套接字：
        socket(AF_INET, SOCK_STREAM, 0)：创建一个 TCP 套接字。
        如果返回值小于 0，表示创建失败，打印错误信息并退出。
        设置服务器地址：
        inet_pton(AF_INET, argv[1], &(cli.sin_addr))：将命令行参数中的 IP 地址转换为网络字节序，并存储到 cli.sin_addr 中。
    连接服务器：
        connect(sc, (struct sockaddr*)&cli, sizeof(cli))：尝试连接到服务器。
        如果返回值小于 0，表示连接失败，打印错误信息并退出。
        调用 process 函数：
        处理客户端与服务器的通信。
    关闭套接字：
        在 process 函数退出后，关闭客户端套接字。
*/
int main(int argc, char *argv[])
{
    assert(argc == 2);

    struct sockaddr_in cli;
    bzero(&cli, sizeof(cli));
    cli.sin_family = AF_INET;
    cli.sin_addr.s_addr = htonl(INADDR_ANY);
    cli.sin_port = htons(PORT);                     // 少了 htons 的话就连接不上了，因为小端机器的原因？？？

    int sc = socket(AF_INET, SOCK_STREAM, 0);
    if(sc < 0) {
        perror("socket error");
        exit(-1);
    }
    inet_pton(AF_INET, argv[1], &(cli.sin_addr));           // 用第一个参数作为连接服务器端的地址

    int err = connect(sc, (struct sockaddr*)&cli, sizeof(cli));
    if(err < 0) {
        perror("connect error");
        exit(-2);
    }
    process(sc);
    close(sc);

    return 0;
}
