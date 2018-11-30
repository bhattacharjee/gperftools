#ifndef _LEAK_TRACE_H
#define _LEAK_TRACE_H
void tc_ll_log_malloc(void *ptr, size_t size);
void tc_ll_log_free(void *ptr);
void tc_ll_log_realloc(void *oldptr, void* newptr, size_t size);
void tc_ll_log_memalign(void *ptr, size_t size, size_t align);

void tc_ll_set_inside_subsystem(int i);
int tc_ll_get_inside_subsystem();
#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */
void tc_ll_init();
#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */
#endif /* #ifndef _LEAK_TRACE_H */
