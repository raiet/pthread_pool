#!/bin/sh

gcc -g -o pool thread_pool.c pool_test.c -lpthread
