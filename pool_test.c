#include "pthread_pool.h"
#include <stdio.h>
#include <stdlib.h>

void my_process(void* args){
	tp_work_arg *arg=(tp_work_arg*)args;
	int val=*(int*)arg->args;
	printf("hello :%d\n",val);
	sleep(1);
	printf("%d exit!!\n",val);
}

int main(){
	int num=100;
	int workingnum[100];
	tp_thread_pool *pool=creat_thread_pool(5,20);
	

	tp_work *worker=(tp_work*)malloc(sizeof(tp_work));
	worker->process_job=my_process;
	
	int i;
    for (i = 0; i < num; i++){			
        tp_work_arg *arg=(tp_work_arg *)malloc(sizeof(tp_work_arg));
		workingnum[i]=i;
        arg->args=(void*)&workingnum[i];
		tp_add_job(pool,worker,arg);
     }

    sleep(60);
}

