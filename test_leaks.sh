#!/bin/bash
gcc test_leaks.c -g -rdynamic -o test_leaks -ldl -pthread -L/usr/lib/x86_64-linux-gnu/ && LD_PRELOAD=./.libs/libtcmalloc.so ./test_leaks
 
