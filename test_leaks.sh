#!/bin/bash
cc test_leaks.c -o test_leaks -ldl && LD_PRELOAD=./.libs/libtcmalloc_minimal.so ./test_leaks
