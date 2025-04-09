定时器
*************************************************
文件结构：
*************************************************
.  
|---timer.h  
|---timer.cpp  
*************************************************
类及其接口：
*************************************************
Sorted_timer_list  
|---构造函数---------Sorted_timer_list()  
|---析构函数---------~Sorted_timer_list()  
|---定时器添加函数---add_timer(Util_timer *timer)  
|---定时器调整函数---adjust_timer(Util_timer *timer)  
|---定时器删除函数---del_timer(Util_timer *timer)  
|---定时器处理函数---tick()  
**************************************************
Utils  
|---构造函数--------Utils()  
|---析构函数--------~Utils()  
|---fd非阻塞设置----SetNonBlocking(int fd)  
|---注册epoll事件---Addfd(int epollfd, int fd, bool one_shot, int TRIGMode)  
|---信号处理函数----SigHandler(int sig)  
|---信号设置函数----AddSig(int sig, void(handler)(int), bool restart)  
|---定时器处理函数--TimerHandler()  
|---错误返回函数----ShowError(int connfd, const char *info)  
|---回调函数-------cb_func(client_data *user_data)  
