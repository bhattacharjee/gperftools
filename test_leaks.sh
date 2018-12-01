#!/bin/bash
gcc test_leaks.c -g -rdynamic -o test_leaks -ldl && LD_PRELOAD=./.libs/libtcmalloc_minimal.so ./test_leaks
 
