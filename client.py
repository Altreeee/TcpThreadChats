#!/usr/bin/env python
#-*- coding: utf-8 -*-

import socket
import sys
import thread

BUF_LEN = 1024

'''
recive
参数：client 是客户端的套接字对象。
功能：在一个独立线程中运行，用于接收服务器发送的数据并打印到终端。
逻辑：
循环接收数据（client.recv(BUF_LEN)），每次接收最多 BUF_LEN 字节。
如果接收失败（如连接断开）或接收到空数据，退出循环。
打印接收到的数据（print data）。
关闭套接字并退出程序（sys.exit(0)）。
'''
def recv_func(client):
    while True:
        try:
            data = client.recv(BUF_LEN)
        except:
            break
        if not data or not len(data):
            break
        print data
    client.close()
    sys.exit(0)

'''
参数：client 是客户端的套接字对象。
功能：处理用户输入并发送给服务器，同时启动一个线程接收服务器的消息。
逻辑：
使用 thread.start_new_thread 启动一个新线程运行 recv_func，用于接收服务器消息。
主线程进入循环，等待用户输入（raw_input()）。
如果用户输入 bye，退出循环并关闭连接。
否则，将用户输入的数据发送给服务器（client.sendall(data)）。
如果发送失败（如服务器断开连接），退出循环。
关闭套接字并退出程序。
'''
def process(client):
    thread.start_new_thread(recv_func, (client, ))
    while True:
        try:
            data = raw_input()
            if data == 'bye':
                break
            # data += '\n'
            client.sendall(data)
        except:
            break
    client.close()
    sys.exit(0)

if len(sys.argv) != 3:
    print 'Usage: ./client.py [ip] [port]'
    sys.exit(1)

HOST = sys.argv[1]
PORT = int(sys.argv[2])

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
client.connect((HOST, PORT))

process(client)
client.close()
