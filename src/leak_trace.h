#ifndef _LEAK_TRACE_H
#define _LEAK_TRACE_H
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

#include <execinfo.h>

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
