// by 陈尉
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    /*
            构造函数
            参数：
            - actor_model：模型切换标志，用于选择任务处理方式
            - connPool：数据库连接池对象
            - thread_number：线程池中的线程数量，默认值为 8
            - max_requests：请求队列的最大任务数，默认值为 10000
    */
    threadpool(int actor_model,
               connection_pool *connPool,
               int thread_number = 8,
               int max_request = 10000);

    // 析构函数，用于释放线程资源
    ~threadpool();
    /*
       添加任务到请求队列
       参数：
       - request：任务对象指针
       - state：任务状态（读或写）
       返回值：成功返回 true，失败返回 false
   */
    bool append(T *request, int state);

    /*
        添加任务到请求队列（无状态设置）
        参数：
        - request：任务对象指针
        返回值：成功返回 true，失败返回 false
    */
    bool append_p(T *request);

private:
    /*
            工作线程运行的静态函数，每个线程循环调用此函数以执行任务
            参数：
            - arg：线程池对象指针
            返回值：返回线程池对象指针
    */
    static void *worker(void *arg);

    /*
       实际运行任务的函数，从任务队列中取出任务并执行
   */
    void run();

private:
    int m_thread_number;         // 线程池中的线程数
    int m_max_requests;          // 请求队列允许的最大请求数
    pthread_t *m_threads;        // 描述线程池的线程数组，其大小为 m_thread_number
    std::list<T *> m_workqueue;  // 请求队列，存储待处理任务的指针
    locker m_queuelocker;        // 保护请求队列的互斥锁，保证线程安全
    sem m_queuestat;             // 信号量，标志是否有任务需要处理
    connection_pool *m_connPool; // 数据库连接池对象，用于数据库操作
    int m_actor_model;           // 模型切换标志，决定任务处理方式
};

/*
    构造函数实现
    初始化线程池参数，创建线程并启动
*/
template <typename T>
threadpool<T>::threadpool(
    int actor_model,
    connection_pool *connPool,
    int thread_number = 8,
    int max_request = 10000) : m_actor_model(actor_model),
                               m_connPool(connPool),
                               m_thread_number(thread_number),
                               m_max_requests(max_request),
                               m_threads(NULL)
{
    // 检查线程数和最大请求数是否合法
    if (thread_number <= 0 || max_request <= 10000)
        throw std::exception();

    // 分配线程数组的内存
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();

    // 创建线程并启动
    for (int i = 0; i < m_thread_number; ++i)
    {
        if (pthread_create(m_threads[i], NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }

        // 分离线程，确保线程资源在结束后自动释放
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

/*
    析构函数实现
    释放动态分配的线程数组
*/
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

/*
    添加任务到请求队列，并设置任务状态
*/
template <typename T>
bool threadpool<T>::append(T *request, int state)
{
    // 加锁，保护队列操作
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests) // 检查队列是否已满
    {
        m_queuelocker.unlock();
        return false; // 如果队列已满，返回失败
    }

    // 设置任务状态并把请求加入队列
    m_actor_model = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();

    // 信号量通知线程有新任务
    m_queuestat.post();

    return true;
}

/*
    添加任务到请求队列（无状态设置）
*/
template <typename T>
bool threadpool<T>::append_p(T *request)
{ // 加锁，保护队列操作
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests) // 检查队列是否已满
    {
        m_queuelocker.unlock();
        return false; // 如果队列已满，返回失败
    }

    // 请求加入队列
    m_workqueue.push_back(request);
    m_queuelocker.unlock();

    // 信号量通知线程有新任务
    m_queuestat.post();

    return true;
}

/*
    工作线程的静态函数
    调用线程池对象的 run 方法
*/
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg; // 转换为线程池对象指针
    pool->run();                          // 调用 run 方法
    return pool;                          // 返回线程池对象指针
}
/*
    运行任务的函数，从队列中取出任务并执行
*/
template <typename T>
void threadpool<T>::run()
{
    // 循环处理的线程工作
    while (true)
    {
        // 等待信号量唤醒
        m_queuestat.wait();
        // 加锁，保护队列操作
        m_queuelocker.unlock();
        // 如果队列为空，释放锁并继续循环
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        // 从队列中取出任务
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        // 如果任务为空，跳过处理
        if (!request)
            continue;

        // 根据模型标志决定任务处理方式
        if (1 == m_actor_model) // 模型 1：根据任务状态选择操作
        {
            // 读任务
            if (0 == request->m_state)
            {
                if (request->read_once()) // 读取成功
                {
                    request->improv = 1;
                    // 获取数据库连接
                    connectionRAII mysqln(&request->mysql, m_connPool);
                    // 处理任务
                    request->process();
                }
                else // 读取失败
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            // 写任务
            else
            {
                if (request->write()) // 写入成功
                {
                    request->improv = 1;
                }
                else // 写入失败
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else // 模型 2：直接处理任务
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool); // 获取数据库连接
            request->process();                                   // 处理任务
        }
    }
}

#endif
