#!/bin/bash
cc test_leaks.c -g -o test_leaks -ldl && LD_PRELOAD=./.libs/libtcmalloc_minimal.so ./test_leaks
