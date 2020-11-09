#ifndef _LEAK_TRACE_CC
#define _LEAK_TRACE_CC

#include <pthread.h>
#include <stdio.h>
#include "maybe_threads.h"
#include "leak_trace.h"
#include "leak_symcache.cc"

Symcache symcache;

static int          monitoring;
static FILE*        fd;

static int          io_offload_thread_started;
static int          thread_stopping;


#ifdef HAVE_PTHREAD
#include <pthread.h>
pthread_cond_t      condition           = PTHREAD_COND_INITIALIZER;
pthread_cond_t      thread_finished     = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     lock                = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t     lock2               = PTHREAD_MUTEX_INITIALIZER;

stack_record_ptr_t queue;

pthread_key_t inside_subsystem_key_;

void add_to_stack_record(stack_record_ptr_t ptr)
{
    if (!ptr)
        return;
    do
    {
        ptr->next = queue;
    } while(! COMPARE_AND_SWAP(&queue, ptr->next, ptr));

    pthread_mutex_lock(&lock);
    pthread_cond_signal(&condition);
    pthread_mutex_unlock(&lock);
}

void do_print(stack_record_ptr_t stack_record)
{
    if (!stack_record)
        return;

    fwrite(stack_record->buffer, stack_record->buffer_size, 1, fd);
    if (stack_record->log_stack)
    {
        int i;
        int firstseen = 0;
        for(i = 0; i < stack_record->nptrs; i++)
        {
            fprintf(fd, "%p\n", stack_record->backtrace[i]);
            if (symcache.lookup_record(
                        stack_record->backtrace[i],
                        NULL,
                        NULL) < 0)
                firstseen = 1;
        }

        if (firstseen)
        {
            char** strings = backtrace_symbols(
                    stack_record->backtrace,
                    stack_record->nptrs);
            if (!strings)
                fprintf(stderr, "Could not get backtrace");
            for (i = 0; i < stack_record->nptrs && strings; i++)
            {
                if (symcache.lookup_record(
                            stack_record->backtrace[i],
                            NULL,
                            0) < 0)
                {
                    if (strings[i])
                    {
                        symcache.insert_record(
                                stack_record->backtrace[i],
                                strings[i]);
                        fprintf(fd,
                                "-%p-%s\n",
                                stack_record->backtrace[i],
                                strings[i]);
                    }
                }
            }
            if (strings)
                free(strings);
        }

    }
    fprintf(fd, "\n");
}


void do_log(char* buf, size_t buf_siz, int log_stk)
{
    stack_record_ptr_t stack_record =
        (stack_record_ptr_t)calloc(sizeof(stack_record_t), 1);
    if (!stack_record)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        return;
    }

    stack_record->nptrs = backtrace(stack_record->backtrace, BACKTRACE_SIZE);
    stack_record->log_stack = log_stk;
    memcpy(stack_record->buffer, buf, buf_siz);
    stack_record->buffer_size = buf_siz;

    if (io_offload_thread_started)
    {
        add_to_stack_record(stack_record);
        return;
    }
    else
    {
        do_print(stack_record);
        free(stack_record);
    }
}

void* worker_thread(void* arg)
{
    io_offload_thread_started = TRUE;
    thread_stopping = 0;
    tc_ll_set_inside_subsystem(1);
    fprintf(fd, "=START\n");
    while (1)
    {
        stack_record_ptr_t queue_copy;
        stack_record_ptr_t tmpq = NULL;
        if (0 == pthread_mutex_lock(&lock))
        {
            while (0 == thread_stopping && 0 == queue)
                pthread_cond_wait(&condition, &lock);
            pthread_mutex_unlock(&lock);
        }

        do
        {
            queue_copy = queue;
        } while(! COMPARE_AND_SWAP(&queue, queue_copy, 0));

        // Reverse the queue first since that is the order things came in
        while (queue_copy)
        {
            stack_record_ptr_t tmp = queue_copy;
            queue_copy = queue_copy->next;
            tmp->next = tmpq;
            tmpq = tmp;
        }
        queue_copy = tmpq;



        while (queue_copy)
        {
            stack_record_ptr_t ptr = queue_copy->next;
            do_print(queue_copy);
            free(queue_copy);
            queue_copy = ptr;
        }

        if (thread_stopping && !queue)
        {
            if (NULL != fd)
            {
                fprintf(fd, "=STOP\n");
                fclose(fd);
                fd = NULL;
            }
            io_offload_thread_started = 0;
            thread_stopping = 0;
            monitoring = 0;

            pthread_mutex_lock(&lock2);
            pthread_cond_signal(&thread_finished);
            pthread_mutex_unlock(&lock2);
            
            return NULL;
        }
    }
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
    malloc(1);
#ifndef HAVE_TLS
#if HAVE_PTHREAD
    inside_subsystem_key_ = 0;
    perftools_pthread_key_create(&inside_subsystem_key_, NULL);
#endif
#endif
    pthread_mutex_lock(&lock);
    fd = fopen(filename, "w");
    if (NULL == fd)
    {
        fprintf(
                stderr,
                "Could not open file %s, %d : %s\n",
                filename,
                errno,
                strerror(errno));
        pthread_mutex_unlock(&lock);
        return -1;
    }
    monitoring = 1;
    pthread_mutex_unlock(&lock);

#if HAVE_PTHREAD
    start_worker_thread();
#else
    fprintf(fd, "=START\n");
#endif

    return 0;
}

extern "C"
void tc_unmonitor_leaks()
{
    monitoring = 0;
    if (!io_offload_thread_started && NULL != fd)
    {
        fprintf(fd, "=STOP\n");
        fclose(fd);
        fd = NULL;
    }
    else
    {
        pthread_mutex_lock(&lock);
        thread_stopping = 1;
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&lock);

        pthread_mutex_lock(&lock2);
        pthread_cond_wait(&thread_finished, &lock2);
        pthread_mutex_unlock(&lock2);
    }

#ifndef HAVE_TLS
#if HAVE_PTHREAD
    perftools_pthread_key_delete(inside_subsystem_key_);
    inside_subsystem_key_ = 0;
#endif
#endif
}

static int is_recursive()
{
    if (tc_ll_get_inside_subsystem())
        return TRUE;
    tc_ll_set_inside_subsystem(1);
    return FALSE;
}


#define PRINT_SELF() \
    fprintf(fd, "=0x%llx\n", (unsigned long long)pthread_self());

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

void tc_ll_destroy_inside_subsystem_key(void* ptr) {
    return;
}

long tc_ll_get_inside_subsystem()
{
#ifdef HAVE_TLS
    return !! ll_inside_subsystem;
#else /* ifdef HAVE_TLS */
    return (long)PTHREAD_GETSPECIFIC(inside_subsystem_key_);
#endif /* ifdef HAVE_TLS */
}

void tc_ll_set_inside_subsystem(long i)
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

