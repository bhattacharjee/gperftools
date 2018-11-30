#ifndef _LEAK_TRACE_H
#define _LEAK_TRACE_H
void tc_ll_log_malloc(void *ptr, size_t size);
void tc_ll_log_free(void *ptr);
void tc_ll_log_realloc(void *oldptr, void* newptr, size_t size);
void tc_ll_log_memalign(void *ptr, size_t size, size_t align);
#endif /* #ifndef _LEAK_TRACE_H */
