#ifndef _LEAK_TRACE_CC
#define _LEAK_TRACE_CC
#include "config.h"
// At least for gcc on Linux/i386 and Linux/amd64 not adding throw()
// to tc_xxx functions actually ends up generating better code.
#define PERFTOOLS_NOTHROW
#include <gperftools/tcmalloc.h>

/* 
 * TODO:
 * If not using gperftools, these must be redefined
 * Also tc_ll_init() must be called
 */
#define PTHREAD_KEY_CREATE perftools_pthread_key_create
#define PTHREAD_GETSPECIFIC perftools_pthread_getspecific
#define PTHREAD_SETSPECIFIC perftools_pthread_setspecific

#include <errno.h>                      // for ENOMEM, EINVAL, errno
#if defined HAVE_STDINT_H
#include <stdint.h>
#elif defined HAVE_INTTYPES_H
#include <inttypes.h>
#else
#include <sys/types.h>
#endif
#include <stddef.h>                     // for size_t, NULL
#include <stdlib.h>                     // for getenv
#include <string.h>                     // for strcmp, memset, strlen, etc
#ifdef HAVE_UNISTD_H
#include <unistd.h>                     // for getpagesize, write, etc
#endif
#include <algorithm>                    // for max, min
#include <limits>                       // for numeric_limits
#include <new>                          // for nothrow_t (ptr only), etc
#include <vector>                       // for vector

#include <gperftools/malloc_extension.h>
#include <gperftools/malloc_hook.h>         // for MallocHook
#include <gperftools/nallocx.h>
#include "base/basictypes.h"            // for int64
#include "base/commandlineflags.h"      // for RegisterFlagValidator, etc
#include "base/dynamic_annotations.h"   // for RunningOnValgrind
#include "base/spinlock.h"              // for SpinLockHolder
#include "central_freelist.h"  // for CentralFreeListPadded
#include "common.h"            // for StackTrace, kPageShift, etc
#include "internal_logging.h"  // for ASSERT, TCMalloc_Printer, etc
#include "linked_list.h"       // for SLL_SetNext
#include "malloc_hook-inl.h"       // for MallocHook::InvokeNewHook, etc
#include "page_heap.h"         // for PageHeap, PageHeap::Stats
#include "page_heap_allocator.h"  // for PageHeapAllocator
#include "span.h"              // for Span, DLL_Prepend, etc
#include "stack_trace_table.h"  // for StackTraceTable
#include "static_vars.h"       // for Static
#include "system-alloc.h"      // for DumpSystemAllocatorStats, etc
#include "tcmalloc_guard.h"    // for TCMallocGuard
#include "thread_cache.h"      // for ThreadCache

#include "maybe_emergency_malloc.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "leak_trace.h"

#define TRUE 1
#define FALSE 0


#if (defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)) && !defined(WIN32_OVERRIDE_ALLOCATORS)
# define WIN32_DO_PATCHING 1
#endif

// Some windows file somewhere (at least on cygwin) #define's small (!)
#undef small

using STL_NAMESPACE::max;
using STL_NAMESPACE::min;
using STL_NAMESPACE::numeric_limits;
using STL_NAMESPACE::vector;


using tcmalloc::AlignmentForSize;
using tcmalloc::kLog;
using tcmalloc::kCrash;
using tcmalloc::kCrashWithStats;
using tcmalloc::Log;
using tcmalloc::PageHeap;
using tcmalloc::PageHeapAllocator;
using tcmalloc::SizeMap;
using tcmalloc::Span;
using tcmalloc::StackTrace;
using tcmalloc::Static;
using tcmalloc::ThreadCache;

#ifndef _WIN32   // windows doesn't have attribute_section, so don't bother
extern "C" {
  int tc_monitor_leaks(char* filename) PERFTOOLS_NOTHROW
      ATTRIBUTE_SECTION(google_malloc);
  void tc_unmonitor_monitor_leaks() PERFTOOLS_NOTHROW
      ATTRIBUTE_SECTION(google_malloc);
}
#endif /* #ifndef _WIN32 */

static int fd;
static int monitoring;

int tc_monitor_leaks(char *filename)
{
    fd = open(filename, O_CREAT|O_RDWR, S_IRWXU|S_IRWXG);
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

    monitoring = 1;
    fprintf(stderr, "started monitoring\n");
    return 0;
}

void tc_unmonitor_monitor_leaks()
{
    monitoring = 0;
    if (0 != fd && -1 != fd)
    {
        close(fd);
        fd = 0;
    }
    fprintf(stderr, "stopped monitoring\n");
}

static int is_recursive()
{
    if (tc_ll_get_inside_subsystem())
        return TRUE;
    tc_ll_set_inside_subsystem(1);
    return FALSE;
}

#define PROLOG if (!monitoring || is_recursive()) return;
#define EPILOG tc_ll_set_inside_subsystem(0);

void tc_ll_log_malloc(void *ptr, size_t size)
{
    char buffer[128];
    int  bytes;
    PROLOG;
    bytes = snprintf(
            buffer,
            sizeof(buffer),
            "+MALLOC(%llu) = %p\n",
            (unsigned long long)size,
            ptr);
    if (!bytes)
    {
        fprintf(stderr, "Failed in fprintf %d\n", (int)__LINE__);
    }
    write(fd, buffer, bytes);
    EPILOG;
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

