# mymuduo

采用C++11重写`muduo`网络库核心功能，无需依赖于`boost`库，在编译阶段更加简化，并且只需包含` <mymuduo/TcpServer.h>`头文件即可实现服务器编程

## 开发环境

- Linux kernel version 6.2.0-34-generic
- Ubuntu 22.04
- GCC version 11.4.0
- cmake version 3.22.1

## 使用说明

执行`sudo ./autobuild.sh`进行mymuduo的编译安装，头文件自动生成至`/usr/include/mymuduo/`，动态库`libmymuduo.so`文件自动添加到`/usr/lib/`

测试用例在`example/`目录，进入目录执行`make`可生成测试可执行文件

使用`mymuduo`进行服务器编程时，只需包含` <mymuduo/TcpServer.h>`头文件

## 核心模块介绍

1. `Channel`模块实现对`fd`、`events`、`revents`、`callbacks`等成员的封装，方便添加至`Poller`中。

   主要监听两种`Channel`，`acceptorChannel`中的`listenfd`和`connectionChannel`中的`connfd`，另外每个`subLoop`都注册了一个`wakeupFd`封装成`wakeupChannel`，用于有事件到来时唤醒对应的`subLoop`。`TcpServer`的读、写、关闭和错误事件回调最终都绑定至`Channel`的回调函数。

2. `Poller`和`EpollPoller`模块实现对事件的监听，将`Channel`添加至`epoll`中，`epoll_wait`等待事件发生，有事件发生后通过`fillActiveChannels`将发生的事件传回给`Channel.revents`，通过`Channel.revents`的具体事件执行相应的回调函数

3. `EventLoop`模块对应`Reator`反应堆，用于开启事件循环，封装`Channel`、`Poller`，实现事件的轮询检测以及事件分发处理

4. `Thread`、`EventLoopThread`、`EventLoopThreadPoll`实现线程池的封装，采用轮询算法获取下一个`subLoop`，每一个`thread`运行一个`EventLoop`，实现`one loop per thread`

5. `Acceptor`封装了`listenfd`相关操作，运行在`mainLoop`中监听新连接

6. `TcpConnection`用于创建客户端连接，一个连接成功的客户端对应一个`TcpConnection`，其中封装了连接建立、连接关闭、处理读写时间等大量回调函数

7. `TcpServer`为对外服务器编程使用的类，`start()`开启`mainLoop`后，创建`loop`线程池并且`mainLoop`开始监听，每个线程都开始运行一个`EventLoop`，有新连接到来通过轮询分发至某一个`EventLoop`

8. `TimerQueue`、`TimerId`、`Timer`三个定时器类组成了网络库的定时器。一个`TimerQueue`关联一个`EventLoop`，一个`TimerQueue`绑定一个`timerfd_create()`创建出的类似文件描述符的`timerfd`和封装他的`Channel`。

9. `Connector`类，类似于`Acceptor`类，用于给`TcpClient`创建监听`connfd`用于通信，值得注意的是，`Connector`并不持有`connfd`，而是在`newConnectionCallback_`回调中把`connfd`的所有权给了`TcpClient`，因为`Connector`类与`TcpClient`在相同的loop中，只能存在一份connfd和封装他的`Channel`.

10. `TcpClient`类用于创建客户端，功能与`TcpServer`类似，不同的是`TcpClient`只需要专注于连接的建立与断开，信息的收发等功能。

## 技术亮点

1. 采用C++11重写`muduo`网络库核心功能，无需依赖于`boost`库，在编译阶段更加简化，并且只需包含` <mymuduo/TcpServer.h>`头文件即可实现服务器编程

2. `muduo`采用`Reactor`模型和多线程结合的方式，实现了高并发非阻塞网络库。采用大量回调函数使得业务代码和核心编程代码分离，用户使用时只需要在编程时设置`ConnectionCallback`、`MessageCallback`、`WriteCompleteCallback`回调函数，`muduo`库触发相应条件时会自动调用用户设置的函数

3. `EventLoop`中采用系统调用`eventfd`创建一个`wakeupfd`快速实现事件的等待于通知，`wakeupfd`绑定`EPOLLIN`读事件，当`mainLoop`需要唤醒`subLoop`时，向`wakeupfd`写入数据即可唤醒，增加了通知效率

4. `Thread`中的`EventLoop`运行在栈上，通过条件变量确保获取运行的`EventLoop`指针，大大减小分配在堆中的空间，并且自动释放，避免出现内存泄漏

5. `Buffer`模块API设置为直接传入`string`而不是`Buffer`对象，便于用户调用

6. `Logger`日志模块采用格式化字符串方式输出，并且提供用户设置日志等级，在其他编程时也可以方便调用

7. `StringPiece.h`模块采用谷歌设计的接口，允许客户轻松传入一个`const char*`或者`string`，指向另一块内存的类字符串对象。

## TODO

- [x] example目录下增加测试案例，已增加`simepleTCP`几个简单的TCP测试案例，`chatServer`简单的聊天服务器

- [x] 定时器相关类

- [x] `TcpClient`编写客户端类

- [x] 支持HTTP等，现已支持HTTP，`http`目录下有简单的HTTP测试代码

- [ ] 服务器性能测试 