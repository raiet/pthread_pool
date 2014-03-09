#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

typedef struct tp_work_arg_s tp_work_arg;
typedef struct tp_work_s tp_work;
typedef struct tp_thread_info_s tp_thread_info;
typedef struct tp_thread_pool_s tp_thread_pool;

//线程参数
struct tp_work_arg_s{
        void *args;
};

//线程的工作函数
struct tp_work_s{
	void (*process_job)(void*arg);
};

//线程结构，主要包含单个线程的信息
struct tp_thread_info_s{
	pthread_t		thread_id;	//线程id

	int  		status;	//线程状态: 1:忙  2：闲 
	int need_exit;//线程退出标志  1：需要退出，0：不需要退出
	//当前线程的条件变量和互斥锁
	pthread_cond_t          thread_cond;
	pthread_mutex_t		thread_lock;
	//线程的处理函数
	tp_work			*th_work;
	//处理函数的参数
	tp_work_arg		*th_arg;
	struct tp_thread_info_s *next,*pre;
};

//线程池结构
struct tp_thread_pool_s{

	int min_th_num;		//线程池中线程的最小值
	int cur_th_num;		//线程池中当前的线程数
	int max_th_num;      //线程池中允许的最大线程数
	int idle_th_num;			//线程中的空闲的线程数


	//线程池的互斥锁
	pthread_mutex_t tp_lock;
	//管理线程的线程ID
	pthread_t manage_thread_id;	
	//线程结构的数组
	tp_thread_info *thread_info;	//工作线程链表
};


//创建线程池
tp_thread_pool *creat_thread_pool(int min_num, int max_num);
//向线程池添加任务
int tp_add_job(tp_thread_pool *this, tp_work *worker, tp_work_arg *arg);


#endif

