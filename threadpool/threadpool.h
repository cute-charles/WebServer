#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <exception>
#include <list>
#include <pthread.h>
#include <cstdio>
#include "../lock/locker.h"

//模板线程池类
template <typename T>
class threadpool
{
public:
    threadpool(int m_thread_number = 8, int m_max_requests = 10000);
    ~threadpool();
    //请求队列中添加任务
    bool append(T *request);
    //工作队列中取出任务 pthread_create要求为静态函数
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中线程数目
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //线程池的数组表示
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //队列的互斥锁
    sem m_queuestat;            //任务处理信号
    bool m_stop;                //结束线程标志
};

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : m_thread_number(thread_number),
                                                                m_max_requests(max_requests), m_threads(NULL), m_stop(false)
{
    if (thread_number <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }
    //创建线程 并设置为脱离线程
    for (int i = 0; i < m_thread_number; ++i)
    {
        printf("create the %dth thread\n", i);
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        { // 标识符指针 线程属性 运行函数地址 运行函数参数
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool <T>::append(T *request)
{
    m_queuelocker.lock();
    printf("add one request\n");
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
            continue;
        if (1 == m_actor_model)
        {
            if (0 == request->m_state)
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}

template<typename T>
void* threadpool<T> ::worker(void* arg){
    threadpool* pool = (threadpool* )arg;
    pool->run();
    return pool;
}

#endif