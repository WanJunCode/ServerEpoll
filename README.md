# ServerEpoll
epoll [enhancement poll]

this is a async server base on epoll at Linux
这是一个在Linux系统上基于epoll机制的异步服务器

class EpollService 主要功能就是，创建一个后台服务器监听指定端口号，并且接受来自客户端的连接请求。

EpollService 使用两个 epoll 机制来完成对客户端连接的监听以及对客户端数据的读取。
其中 监听端口的线程运行在主线程（listen thread）中，在开启监听epoll前开启一个读取线程（receive thread）。

监听线程 和 读取线程之间使用管道 pipe 完成数据的传输工作。即读取到新的客户端连接之后，将相关信息打包成 pipemsg（管道数据）通过管道pipe机制发送给读取线程。

读取线程主要监听两种事件：
1、管道的读取事件:
    从管道读取到新的pipemsg后将新连接的相关信息存入 std::map fd_info 中，并将该clientfd的读取事件添加到读取epollfd中。
2、客户端的读取事件
    从客户端读取到数据后，对数据进行处理。目前打印出相关信息。
    如果客户端主动断开连接，读取后的数据长度为0，并将相关信息从std::map fd_info中移除。

客户端使用ET模式读取数据，注意一下几点
1、新接收的clientfd设置为 nonblocking
2、读取数据时一定要读取完毕，errno == EAGAIN
3、删除数据时 记得删除 EPOLLET 模式