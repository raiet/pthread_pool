pool : 
	cc -o pool thread_pool.c pool_test.c -lpthread
.PHONY : clean
clean :
	rm -f pool

