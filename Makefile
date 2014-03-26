pool:pthread_pool.o pool_test.o
	cc -o pool pthread_pool.o pool_test.o -lpthread
pthread_pool.o:pthread_pool.c pthread_pool.h
	cc -c -o pthread_pool.o pthread_pool.c
pool_test.o:pool_test.c
	cc -c -o pool_test.o pool_test.c
.PHONY : clean
clean :
	rm -f pool pool_test.o pthread_pool.o


