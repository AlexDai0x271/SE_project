#ifndef LST_TIMER_H
#define LST_TIMER_H
/*
头文件定义
@function: 使用到的头文件
*/
#include <unistd.h>
#include <signal.h>
//用于信号处理
#include <sys/types.h>
#include <sys/epoll.h>
//用于管理epoll事件
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
//用于管理socket地址
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>
//用于时间管理
#include "../log/log.h"

/*
类的前向声明
*/
class Util_timer;
class Utils;
class Sorted_timer_list;

/*
struct: client_data
客户端数据结构体定义
@function: 这里定义了一个结构体，包含了socket地址、socket文件描述符和定时器
@ param address: socket地址
@ param sockfd: socket文件描述符
@ param timer: 定时器
*/
struct client_data
{
    sockaddr_in address;
    int sockfd;         
    Util_timer *timer;  
};

/*
class: Util_timer
定时器节点类实现
@ function: 实现一个定时器节点
public:
@ param Time_out: 过期时间
@ type time_t

@ param cb_func: 回调函数
@ type void (*)(client_data *)

@ param user_data: 用户数据
@ type client_data *

@ param prev: 前一个定时器节点
@ type Util_timer *

@ param next: 后一个定时器节点
@ type Util_timer *

@ func: Util_timer()
@ return: 无
*/
class Util_timer
{
public:
    Util_timer() : prev(NULL), next(NULL) {}
    time_t Time_out;
    void (* cb_func)(client_data *);
    client_data *user_data;
    Util_timer *prev;
    Util_timer *next;
};

/*
class: Sorted_timer_list
升序链表容器实现
@ function: 实现一个升序链表
public:
函数：
@ func: Sorted_timer_list()
@ return: 无
@ function: 初始化链表

@func: ~Sorted_timer_list()
@ return: 无
@ function: 删除链表

@ func: add_timer()
@ param timer: (util_timer *)
@ return: 无
@ function: 添加定时器节点到定时器容器内

@func: adjust_timer()
@ param timer: (util_timer *)
@ return: 无
@ function: 调整定时器链表容器内的顺序

@ func: del_timer()
@ param timer: (util_timer *)
@ return: 无
@ function: 删除定时器节点

@ func: tick()
@ param: 无
@ return: 无
@ function: 定时器到期后，调用回调函数

private: 

函数：
@ func: add_timer()
@ param timer: (util_timer *)
@ param list_head: (util_timer *)
@ return: 无
@ function: add_timer()的私有重写函数，负责添加定时器节点到链表内

字段：
@ param head: (util_timer *)
@ function: 链表头节点

@ param tail: (util_timer *)
@ function: 链表尾节点
*/
class Sorted_timer_list{
public:
    Sorted_timer_list();

    ~Sorted_timer_list();

    void add_timer(Util_timer *timer);

    void adjust_timer(Util_timer *timer);

    void del_timer(Util_timer *timer);

    void tick();

private:
    void insert_timer(Util_timer *timer, Util_timer *list_head);

    Util_timer *head;

    Util_timer *tail;
};

/*
class: Utils
工具类（定时器、信号处理、文件描述符设置非阻塞）
public: 

函数：
@ func: Utils()
@ return: 无

@ func: ~Utils()
@ return: 无

@ func: Init()
@ param timeslot: (int)
@ function: 初始化定时器
@ return: 无

@ func: SetNonBlocking()
@ param fd: (int) websocket文件描述符
@ function: 设置文件描述符为非阻塞
@ return: 0成功，-1失败

@ func: Addfd()
@ param epollfd: (int) epoll文件描述符
@ param fd: (int) websocket文件描述符
@ param one_shot: (bool) 是否开启EPOLLONESHOT
@ param TRIGMode: (int) 触发模式
@ function: 将文件描述符添加到epoll内，触发方式设置为ET模式，选择开启EPOLLONESHOT
@ return: 无

@ func: Sig_handler() 静态函数
@ param sig: (int) 信号名
@ function: 信号处理函数
@ return: 无

@ func: Add_Sig()
@ param sig: (int) 信号名
@ param handler: (void *) 信号处理函数
@ param restart: (bool) 是否重启，默认重启
@ function: 设置信号处理函数
@ return: 无

@ func: Timer_handler()
@ param: 无
@ function: 定时器处理任务，触发定时器链表的tick()函数，不断触发SIGALRM信号
@ return: 无

@ func: Show_error()
@ param connfd: (int) websocket文件描述符
@ param info: (const char *) 错误信息
@ function: 显示错误信息
@ return: 无

字段：
@ param m_TIMESLOT: (int)
@ function: 定时器时间间隔

@ param u_pipefd: (static int *)
@ function: 管道文件描述符

@ param m_Timer_list: (Sorted_timer_list)
@ function: 定时器链表容器

@ param u_epollfd: (static int)
@ function: epoll文件描述符
*/
class Utils{
public:
    Utils(){};
    ~Utils(){};

    void Init(int timeslot);

    int SetNonBlocking(int fd);

    void Addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    static void SigHandler(int sig);

    void AddSig(int sig, void(handler)(int), bool restart = true);

    void TimerHandler();

    void ShowError(int connfd, const char *info);

    int m_TIMESLOT;

    static int *u_pipefd;

    Sorted_timer_list m_timer_list;

    static int u_epollfd;
}