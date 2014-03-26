/*
 这个线程池是根据具体项目设计的，并没有严格的考虑有些地方的加锁，因为即使是个别的出错，不会影响任务的正常执行，以一定的误差来提高效率！！
 线程池的动态变化也是采取了一个简单的策略：连续统计三次的平均空闲线程数量如果大于当前总线程数的一半，则需要销毁当前线程的1/5，以防止这种情况只是暂时的，而频繁的销毁和创建线程
 关于管理线程的管理频率和动态更新策略，根据实际需要需要修改，
 这二个线程池中没有设置任务队列，因为实际项目中的任务都是已经保存的，不会丢失，线程的主要任务就是处理任务，并删除任务，所以没有必要设置任务队列

 */
#include"pthread_pool.h"


tp_thread_info* malloc_pthread(int pthread_num);
tp_thread_pool *creat_thread_pool(int min_num, int max_num);
int creat_thread(tp_thread_info* thread_info,tp_thread_pool *this);
int tp_init(tp_thread_pool *this);
int tp_add_job(tp_thread_pool *this, tp_work *worker, tp_work_arg *arg);
tp_thread_info *tp_add_thread(tp_thread_pool *this);
static void *tp_work_thread(void *pthread);
tp_thread_info* get_thread_by_pid(tp_thread_pool* pool,pthread_t pid);
void tp_destory(tp_thread_pool *this);
static void *tp_manage_thread(void *pthread);

/**
  *创建给定数目的线程的线程结构的空间，返回一个头指针
  *参数：
        pthread_num:要分配空间的线程数
  *返回：
        
*/

tp_thread_info* malloc_pthread(int pthread_num){
		tp_thread_info* head,*cur,*pre;

		head = (tp_thread_info*)malloc(sizeof(tp_thread_info));
		//head->pre = NULL;
		pthread_num--;
		
		cur = head;
		pre = NULL;
		while(pthread_num){
				cur->pre = pre;
				cur->next = (tp_thread_info*)malloc(sizeof(tp_thread_info));
				pre = cur;
				cur = cur->next;
				pthread_num--;
		}
		cur->next = NULL;

		return head;
}

/**
    *	创建线程池
	*	参数：
		min_num:线程池的最小线程数量，初始化时，当前线程数也是最小值
		max_num:允许创建的线程的最大值
	*	返回：
			成功：线程池结构的指针
			失败：NULL
*/
tp_thread_pool *creat_thread_pool(int min_num, int max_num){
	tp_thread_pool *this = NULL;

	this = (tp_thread_pool*)malloc(sizeof(tp_thread_pool));	
	if(NULL == this)
		return NULL;

	memset(this, 0, sizeof(tp_thread_pool));
	//修正传入的参数
	if(min_num < 3)
		min_num = 3;
	if(max_num > 200)
		max_num = 200;
	
	//初始化线程池大小
	this->min_th_num = min_num;
	this->cur_th_num = this->min_th_num;
	this->max_th_num = max_num;
	this->idle_th_num = this->cur_th_num;

	//初始化线程池互斥锁
	pthread_mutex_init(&this->tp_lock, NULL);

	//根据线程的最大值，为线程结构分配空间
	if(NULL != this->thread_info)
		free(this->thread_info);
	this->thread_info = malloc_pthread(this->cur_th_num);

	if(NULL == this->thread_info){
		free(this);
		return NULL;
	}
	//初始化线程池
	tp_init(this);
	
	return this;
}

/**
  *创建并初始化线程(这里初始化的是首次创建的线程)
  *参数：
        thread_info:需要初始化的线程的结构体指针
        this：线程池结构
   *返回：
        成功返回1，失败返回0

*/
int creat_thread(tp_thread_info* thread_info,tp_thread_pool *this){
		int err;
        //初始化 cond&mutex
		pthread_cond_init(&thread_info->thread_cond, NULL);
		pthread_mutex_init(&thread_info->thread_lock, NULL);
		
		err = pthread_create(&thread_info->thread_id, NULL, tp_work_thread, this);
		if(0 != err){
			printf("tp_init: creat work thread failed\n");
			return 0;
		}
		//设置线程分离
		pthread_detach(thread_info->thread_id);
		thread_info->status = 2;//空闲
		return 1;
}

/**
  *初始化线程结构
  *参数:
 		this: 线程池的指针
  *返回:
     	true: 1; 
        false: 0
  */
int tp_init(tp_thread_pool *this){
	int i;
	int err;
	pthread_t pid;
	tp_thread_info* thread_info;
	
	//创建线程并初始化线程结构
	for(thread_info = this->thread_info;thread_info!=NULL;thread_info = thread_info->next){
		if(creat_thread(thread_info,this))
			printf("tp_init: creat work thread %lu\n", thread_info->thread_id);
		else{
            printf("tp_init: creat thread failed!\n");
			return 0;
		}
	}

	//创建管理线程
	err = pthread_create(&this->manage_thread_id, NULL, tp_manage_thread, this);

	if(0 != err){
		printf("tp_init: creat manage thread failed\n");
		return 0;
	}
	else{
		printf("tp_init: creat manage thread %lu\n", this->manage_thread_id);
		return 1;
	}
}

/**
  *销毁线程池
  *参数：
        this: 线程池结构

*/
void tp_destory(tp_thread_pool *this){
    		
	
}

/**
  *给线程池添加作业
  * para:
  * 	this: 线程池结构指针
  *	    worker:作业要执行的函数
  *	    job：函数的参数 
  * return:
  */
int tp_add_job(tp_thread_pool *this, tp_work *worker, tp_work_arg *arg){
	int i;
	int tmpid;
	tp_thread_info* thread_info,*new_thread;

repeat:	
//	for(i = 0;i<3;i++){//需要遍历三遍链表，防止过快的创建太多线程
		for(thread_info = this->thread_info;thread_info!=NULL;thread_info = thread_info->next){
			if(pthread_mutex_lock(&thread_info->thread_lock)==0){
				if(thread_info->status == 2){
					printf("thread idle, thread id is %lu\n", thread_info->thread_id);
					//设置线程状态
		  			thread_info->status = 1;
					this->idle_th_num--;
					pthread_mutex_unlock(&thread_info->thread_lock);
					
					thread_info->th_work = worker;
					thread_info->th_arg = arg;
					//唤醒该线程
					pthread_cond_signal(&thread_info->thread_cond);
            		return;
	    		}
        	pthread_mutex_unlock(&thread_info->thread_lock);
			}
		}//end of for
//	}
	//如果没有空闲线程考虑增加线程

	pthread_mutex_lock(&this->tp_lock);
	new_thread = NULL;
   
	if( (new_thread = tp_add_thread(this))!= NULL){
		
		new_thread->th_work = worker;
		new_thread->th_arg = arg;
		pthread_cond_signal(&new_thread->thread_cond);
		pthread_mutex_unlock(&this->tp_lock);
        return ;
    }
	pthread_mutex_unlock(&this->tp_lock);
	//printf("没有线程空闲，自动重新调度\n");
	goto repeat;
	//没有空闲的线程
	return;	
}



/**
  * 线程池中增加一个线程
  * 参数:
  * 	this: 线程池结构指针
  * return:
  * 	成功：返回线程结构指针
        失败：返回NULL
  */
tp_thread_info *tp_add_thread(tp_thread_pool *this){
	int err;
	tp_thread_info* new_thread;
	tp_thread_info *last;
	
	new_thread = NULL;
	if( this->max_th_num <= this->cur_th_num )
		return NULL;
	//找到最后一个线程结构
	last = this->thread_info;
	while(last->next){
		last = last->next;
	}
	
	new_thread = (tp_thread_info *)malloc(sizeof(tp_thread_info));
	if(NULL == new_thread)
		return NULL;
	
    last->next = new_thread;
	new_thread->pre = last;

	//初始化 cond & mutex
	pthread_cond_init(&new_thread->thread_cond, NULL);
	pthread_mutex_init(&new_thread->thread_lock, NULL);


	//当前线程数加1
	this->cur_th_num++;
	
	err = pthread_create(&new_thread->thread_id, NULL, tp_work_thread, this);
	if(0 != err){
		free(new_thread);
		return NULL;
	}

	//设置线程分离
	pthread_detach(new_thread->thread_id);
	new_thread->status = 2;//空闲
	printf("tp_add_thread---------------: creat work thread %lu\n", new_thread->thread_id);
	this->idle_th_num++;
	return new_thread;
}


/**
  *销毁一个线程结构，同时将他从线程结构链表中删除
  * 参数
        thread:需要删除的线程结构
  */
void destory_thread(tp_thread_info* thread){
	tp_thread_info* pre,*next;
    //销毁条件变量和互斥锁
	pthread_mutex_destroy(&thread->thread_lock);
	pthread_cond_destroy(&thread->thread_cond);
    if(thread->next == NULL){
        thread->pre->next = NULL;
    }
    else{
	    //从线程链表中删除
	    pre = thread->pre;
	    next = thread->next;
	    pre->next = next;
	    next->pre = pre;
    }
	free(thread);
	
}

/**
  * 工作线程主函数
  * 参数:
  * 	pthread: 需要强转为线程池指针
  * 
  */
static void *tp_work_thread(void *pthread){
	pthread_t pid;
	tp_thread_pool *this = (tp_thread_pool*)pthread;
	tp_thread_info* thread;

    pid = pthread_self();
	
	//get current thread's seq in the thread info struct array.
	thread = get_thread_by_pid(this, pid);
	if(NULL == thread)
		return;
	printf("entering working thread , thread id is %lu\n", pid);
	
	while(1){
		pthread_mutex_lock(&thread->thread_lock);
       while(thread->status != 1){
		    pthread_cond_wait(&thread->thread_cond, &thread->thread_lock);
            if(thread->need_exit == 1)
                break;
       }
		
        //检查是不是需要退出
		if(thread->need_exit){
            printf("pthread %lu  will exit.....\n",pid);
			pthread_mutex_unlock(&thread->thread_lock);
			destory_thread(thread);
			this->cur_th_num--;
			this->idle_th_num--;
			return;
		}
		pthread_mutex_unlock(&thread->thread_lock);
		
		printf("%lu thread do work!\n", pid);
		tp_work *work = thread->th_work;
		tp_work_arg *arg = thread->th_arg;


		work->process_job(arg);

		free(arg);


		//设置线程状态
		pthread_mutex_lock(&thread->thread_lock);		
		thread->status = 2;
		this->idle_th_num++;
		pthread_mutex_unlock(&thread->thread_lock);
		
		printf("%lu do work over\n", pid);
	}	
}

/**
  * 根据线程ID检索线程结构
  *
  * 参数：
        pool:线程池结构指针
        pid: 线程ID
    返回；成功返回线程结构指针
           失败：NULL
 */
tp_thread_info* get_thread_by_pid(tp_thread_pool* pool,pthread_t pid){
	tp_thread_info* cur;
	cur = pool->thread_info;
	while(cur){
		if(pthread_equal(cur->thread_id,pid)){
				return cur;
		}
		cur = cur->next;
    }
	return NULL;
}

/**
  * 管理线程工作函数
  * 参数：
  * 	pool_this: 线程池结构指针
  * 
  */
void *tp_manage_thread(void *pool_this){
	int cur,idle;//当前线程数，当前空闲线程数

    //用于统计三次空闲的线程数
	int idle_arv[3] = {0};
    //三次的平均值
	int average;
    //需要退出的线程数
	int exit_num;
    //线程池的最小值
	int min;
    tp_thread_info*thread_info;

    tp_thread_pool *this = (tp_thread_pool*)pool_this;
	while(1){
		cur = this->cur_th_num;
		idle = this->idle_th_num;
		min = this->min_th_num;
		exit_num = 0;
		idle_arv[2] = idle;
		
		average = (idle_arv[0]+idle_arv[1]+idle_arv[2])/3;
	    printf("*****************************************\n");	
		printf("主机当前运行状态：\n");
		printf("总线程数：%d\n",cur);
		printf("当前空闲线程数：%d\n",idle);
		printf("线程平均空闲数：%d\n",average);
		//空闲线程数大于当前线程数的一半，则销毁当前线程数的1／5
		if(average>(cur/2)){
				exit_num = cur/5;
		}
        //对退出数进行修正
		if(cur-exit_num<min){
				exit_num = cur-min;
		}
        printf("需要销毁 %d 个线程\n",exit_num);    
        printf("*****************************************\n");
        //跳过第一个
	    for(thread_info = this->thread_info->next;thread_info!=NULL&&exit_num>0;thread_info = thread_info->next){
            pthread_mutex_lock(&thread_info->thread_lock);
             //当前线程是否空闲
			if(thread_info->status != 2){
                pthread_mutex_unlock(&thread_info->thread_lock);
				continue;
			}
			thread_info->need_exit = 1;
            pthread_mutex_unlock(&thread_info->thread_lock);
			exit_num--;
			pthread_cond_signal(&thread_info->thread_cond);
		}
		
        //统计值前移
		idle_arv[0] = idle_arv[1];
		idle_arv[1] = idle_arv[2];
        //每5秒做一次统计
		sleep(5);
	}
}
	
