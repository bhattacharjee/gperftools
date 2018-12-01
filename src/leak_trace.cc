#ifndef _LEAK_TRACE_CC
#define _LEAK_TRACE_CC

#include "leak_trace.h"

static int monitoring;
static int fd = -1;

static int io_offload_thread_started;
static int thread_stopping;

#ifdef HAVE_PTHREAD
#include <pthread.h>
pthread_cond_t  condition;
pthread_mutex_t lock;

void* worker_thread(void* arg)
{
    //io_offload_thread_started = TRUE;
    return NULL;
}

int start_worker_thread()
{
    int                 res;
    static pthread_t   th;

    thread_stopping = FALSE;

    res = pthread_create(
            &th,
            NULL,
            worker_thread,
            0);
    if (0 != res)
    {
        fprintf(stderr,
                "Failed to create worker thrad %d",
                (int)__LINE__);
    }
    return res;
}
#else /* #ifdef HAVE_PTHREAD */
#endif /* #ifdef HAVE_PTHREAD */

extern "C"
int tc_monitor_leaks(char *filename)
{
    fd = open(filename, O_CREAT|O_RDWR|O_TRUNC, S_IRWXU|S_IRWXG);
    if (-1 == fd)
    {
        fprintf(
                stderr,
                "Could not open file %s, %d : %s\n",
                filename,
                errno,
                strerror(errno));
        return -1;
    }

#if HAVE_PTHREAD
    start_worker_thread();
#endif

    write(fd, "=START\n", strlen("=START\n"));
    monitoring = 1;
    return 0;
}

extern "C"
void tc_unmonitor_leaks()
{
    monitoring = 0;
    if (0 != fd && -1 != fd)
    {
        write(fd, "=STOP\n", strlen("=STOP\n"));
        if (!io_offload_thread_started)
        {
            close(fd);
            fd = -1;
        }
        io_offload_thread_started = 0;
    }
}

static int is_recursive()
{
    if (tc_ll_get_inside_subsystem())
        return TRUE;
    tc_ll_set_inside_subsystem(1);
    return FALSE;
}


#define PRINT_SELF() \
    char self[64]; \
    int byt = sprintf(self, "=0x%llx\n", (unsigned long long)pthread_self()); \
    write(fd, self, byt);
    

#define BACKTRACE_SIZE 25
void do_log(char* buf, size_t buf_siz, int log_stk)
{
    void*   bktrc[BACKTRACE_SIZE];
    int     nptrs;

    nptrs = backtrace(bktrc, BACKTRACE_SIZE);
    if (io_offload_thread_started)
    {
        return;
    }
    else
    {
        write(fd, buf, buf_siz);
        if (log_stk)
        {
            backtrace_symbols_fd(bktrc, nptrs, fd);
#if 0
            int i;
            char** strings = backtrace_symbols(bktrc, nptrs);
            for (i = 0; i < nptrs; i++)
            {
                write(fd, strings[i], strlen(strings[i]));
                write(fd, "\n", 1);
                free(strings[i]);
            }
            free(strings);
#endif
        }
        write(fd, "\n", 1);
    }
}

#define PROLOG \
    char buffer[128]; \
    int bytes;\
    if (!monitoring || is_recursive()) return;

#define EPILOG(print_stack) \
    if (!bytes) \
    { \
        fprintf(stderr, "Failed in fprintf %d\n", (int)__LINE__); \
    } \
    do_log(buffer, bytes, print_stack); \
    tc_ll_set_inside_subsystem(0);


void tc_ll_log_malloc(void *ptr, size_t size)
{
    PROLOG;
    bytes = snprintf(
            buffer,
            sizeof(buffer),
            "+MALLOC(%llu) = %p\n",
            (unsigned long long)size,
            ptr);
    EPILOG(1);
}

void tc_ll_log_free(void *ptr)
{
    PROLOG;
    bytes = snprintf(
            buffer,
            sizeof(buffer),
            "+FREE(%p)\n",
            ptr);
    EPILOG(0);
}

void tc_ll_log_realloc(void *oldptr, void* newptr, size_t size)
{
    PROLOG;
    bytes = snprintf(
            buffer,
            sizeof(buffer),
            "+REALLOC(%p, %llu) = %p\n",
            oldptr,
            (unsigned long long)size,
            newptr);
    EPILOG(1);
}

void tc_ll_log_memalign(void *ptr, size_t size, size_t align)
{
    PROLOG;
    bytes = snprintf(
            buffer,
            sizeof(buffer),
            "+MEMALIGN(%llu, %llu) = %p\n",
            (unsigned long long)size,
            (unsigned long long)align,
            ptr);
    EPILOG(1);
}

#ifdef HAVE_TLS
static __thread int ll_inside_subsystem
# ifdef HAVE___ATTRIBUTE__
   __attribute__ ((tls_model ("initial-exec")))
# endif
;
#endif /* #ifdef HAVE_TLS */
static pthread_key_t inside_subsystem_key_;

void tc_ll_destroy_inside_subsystem_key(void* ptr) {
    return;
}

int tc_ll_get_inside_subsystem()
{
#ifdef HAVE_TLS
    return !! ll_inside_subsystem;
#else /* ifdef HAVE_TLS */
    return (int)PTHREAD_GETSPECIFIC(inside_subsystem_key_);
#endif /* ifdef HAVE_TLS */
}

void tc_ll_set_inside_subsystem(int i)
{
#ifdef HAVE_TLS
    ll_inside_subsystem = i;
#else /* ifdef HAVE_TLS */
    PTHREAD_SETSPECIFIC(inside_subsystem_key_, (void*)i);
#endif /* ifdef HAVE_TLS */
}

extern "C"
void tc_ll_init()
{
  PTHREAD_KEY_CREATE(
          &inside_subsystem_key_,
          tc_ll_destroy_inside_subsystem_key);
}
#endif /* #ifndef _LEAK_TRACE_CC */

