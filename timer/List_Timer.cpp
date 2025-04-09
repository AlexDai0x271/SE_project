#include "List_Timer.h"
#include "../http/http_conn.h"

/*
@ function: 基础构造函数
@ return: 无
*/
Sorted_timer_list::Sorted_timer_list(){
    head = NULL;
    tail = NULL;
}

/*
@ function: 基础析构函数
@ return: 无
*/
Sorted_timer_list::~Sorted_timer_list(){
    Util_timer *temp = head;
    while(temp){
        head = temp->next;
        delete temp;
        temp = head;
    }
}
/**********Sorted_timer_list********** */
/*
@ function: 在链表内添加定时器
@ param: timer (Util_timer *)
@ return: 无
*/
void Sorted_timer_list::add_timer(Util_timer *timer){
    //调用私有插入函数实现插入算法
    insert_timer(timer, head);
}

/*
@ function: 插入定时器
@ param: timer (Util_timer* )
@ param: list_head (Util_timer* )
@ return: 无
*/
void Sorted_timer_list::insert_timer(Util_timer *timer, Util_timer *list_head){
    if(!timer){
        return;
    }

    if (!list_head){
        head = tail = timer;
        timer->next = timer->prev = NULL;
        return;
    }
    
    //如果新建的节点的超时时间小于头节点那么就头插进头节点
    if(timer->Time_out < list_head->Time_out){
        timer -> next = list_head;
        timer -> prev = NULL;
        list_head -> prev = timer;
        head = timer;
        return;
    }
    
    //如果新建的节点的超时时间大于等于尾节点那么就尾插进尾节点
    if(timer->Time_out >= tail->Time_out){
        timer -> prev = tail;
        timer -> next = NULL;
        tail -> next = timer;
        tail = timer;
        return;
    }

    //中间插入，根据timer的超时时间与头节点还是尾节点的超时时间的距离选择合适的方向进行遍历
    if((timer -> Time_out - list_head ->Time_out) < (tail -> Time_out - timer -> Time_out)){
        Util_timer *temp = head;
        while(temp && temp -> Time_out < timer -> Time_out){
            temp = temp -> next;
        }
        timer -> next = temp;
        timer -> prev =temp -> prev;
        temp -> prev -> next = timer;
        temp -> prev = timer;
    }else{
        Util_timer *temp = tail;
        while(temp && temp -> Time_out > timer -> Time_out){
            temp = temp -> prev;
        }
        timer -> prev = temp;
        timer -> next = temp -> next;
        temp -> next = timer;
        temp ->next ->prev = timer;
    }
}

/*
@ function: 定时器调整函数
@ param: timer (Util_timer *)
@ return: 无
*/
void Sorted_timer_list::adjust_timer(Util_timer *timer){
    if(!timer){
        return;
    }

    Util_timer *next_timer = timer -> next;

    //检查timer的Time_out更改后容器是否还满足对超时时间升序
    if (!next_timer || (timer->Time_out < next_timer->Time_out))
    {
        return;
    }

    //将timer从容器中单独提取出来
    if (timer == head)
    {
        head = head -> next;
        head -> prev = NULL;
    }else{
        timer->prev->next = timer -> next;
        if(timer -> next){
            timer->next->prev = timer -> prev;
        }
    }
    timer->next = timer->prev = NULL;

    //重新插入
    insert_timer(timer, next_timer ? next_timer : head);
}

/*
@ function: 定时器删除函数
@ param: timer (Util_timer *)
@ return: 无
*/
void Sorted_timer_list::del_timer(Util_timer *timer){
    if(!timer){
        return;
    }

    //如果只有一个节点
    if(timer == head && timer == tail){
        delete timer;
        head = tail = NULL;
        return;
    }

    //如果timer是头节点
    if(head == timer){
        head = head -> next;
        head -> prev = NULL;
        delete timer;
        return;
    }

    //如果timer是尾节点
    if(timer == tail){
        tail = tail-> prev;
        tail -> next = NULL;
        delete timer;
        return;
    }

    //中间位置的删除与普通的链表删除节点一致
    timer -> prev -> next = timer -> next;
    timer -> next -> prev = timer -> prev;
    delete timer;
}

/*
@ function: 定时器检查函数
@ param: 无
@ return: 无
*/
void Sorted_timer_list::tick(){
    if (!head)
    {
        return;
    }
    
    time_t current_time = time(NULL);
    Util_timer *temp = head;

    //遍历链表找到没有超时的第一个节点
    while (temp)
    {
        //升序链表，表头是Time_out的最小值
        if(current_time < temp->Time_out){
            break;
        }

        //执行回调函数
        temp -> cb_func(temp -> user_data);

        head = temp -> next;

        if(head){
            head -> prev = NULL;
        }

        delete temp;
        temp = head;
    }
        
}


/***************Utils***************** */
/*
@ function: 初始化定时间隔
@ param: timeslot (int) 定时器的定时间隔
@ return: 无
在服务器启动时调用，设置定时间隔
*/
void Utils::Init(int timeslot){
    m_TIMESLOT = timeslot;
}

/*
@ function: 设置fd为非阻塞模式
@ param: fd (int) 
@ return: 无
*/
int Utils::SetNonBlocking(int fd){
    //old_option获取到文件的open标志
    int old_option = fcntl(fd, F_GETFL);
    //添加非阻塞模式文件标识符O_NONBLOCK到open标志位
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int Utils::Addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if (TRIGMode == 1){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    SetNonBlocking(fd);
}

//信号处理函数
void Utils::SigHandler(int sig){
    //存储errno以保证可重入性
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], reinterpret_cast<char*>(&msg), 1, 0);
    errno = save_errno;
}

//设置指定信号 sig 的处理函数
void Utils::AddSig(int sig, void(handler)(int), bool restart){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，每次SIGALRM信号触发后调用tick函数处理到期的定时器，并且充值定时器alarm
void Utils::TimerHandler(){
    m_timer_list.tick();
    alarm(m_TIMESLOT);
}

//向客户端发送错误信息并且关闭链接
void Utils::ShowError(int connfd, const char *info){
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = NULL;
int Utils::u_epollfd = 0;

void cb_func(client_data *user_data){
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, NULL);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}

