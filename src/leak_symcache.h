#ifndef _LEAKS_SYMCACHE_H
#define _LEAKS_SYMCACHE_H

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif /* ifdef HAVE_PTHREAD */

#include <stdlib.h>

#define DEFAULT_BUCKETS 128
#define DEFAULT_MAX_RECORDS 8192

#ifndef OFFSETOF
#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
#endif /* ifndef OFFSETOF */

typedef struct node {
    struct node* next;
    struct node* prev;
} node_t;

typedef struct node* node_ptr_t;

typedef struct snode {
    struct snode* next;
} snode_t;

typedef struct snode* snode_ptr_t;

typedef struct symrecord {
    snode_t     hashnext;
    node_t      lru;
    void*       ptr;        // The pointer to the text section
    char        symbol[1];
} symrecord_t ;

typedef struct symrecord* symrecord_ptr_t;

class Symcache {
    public:
        Symcache();
    private:
#ifdef HAVE_PTHREAD
        pthread_mutex_t m_lock;
        int lock_lock() {return pthread_mutex_lock(&m_lock);}
        int lock_unlock() {return pthread_mutex_unlock(&m_lock);}
        void lock_init() {m_lock = PTHREAD_MUTEX_INITIALIZER;}
#else /* #ifdef HAVE_PTHREAD */
        int lock_lock() {return 0;}
        int lock_unlock() {return 0;}
        void lock_init() {return;}
#endif /* #ifdef HAVE_PTHREAD */

    private:
        size_t      n_buckets;
        snode_t*    buckets;
        node_t      lruhead;
        node_t      lrutail;
        int         initialized;
        size_t      max_records;
        size_t      n_entries;
    private:
        size_t hash(void *ptr) {return (size_t)ptr % n_buckets;}

    private:
        int insert_record_unsafe(void* ptr, const char* symbol);
        int remove_record_unsafe(void* ptr);
        char* lookup_record_unsafe(void* ptr);
    public:
        int insert_record(void* ptr, const char* symbol)
        {
            int ret;
            lock_lock();
            ret = insert_record_unsafe(ptr, symbol);
            lock_unlock();
            return ret;
        }

        int remove_record(void* ptr)
        {
            int ret;
            lock_lock();
            ret = remove_record_unsafe(ptr);
            lock_unlock();
            return ret;
        }

        /*
         * Can't return a reference to the symbol as it might
         * get deleted at any point of time.
         *
         * Caller must pass a buffer, and the size of the buffer.
         *
         * On success, 0 is returned
         *
         * If the symbol is not found, -1 is returned
         *
         * If the buffer is not large enough, 1 is returned and the required
         * size of the buffer is stored in the out_size
         *
         * If out or out_size is not valid, 2 is returned
         *
         * TODO: A better solution is to use reference counting, but I'm not
         * implementing this at the moment.
         */
        int lookup_record(void* ptr, char* out, size_t* out_size)
        {
            char*       ret;
            size_t      required        = 0;
            int         ret_out         = 0;

            if (!out || !out_size)
                return 2;

            lock_lock();
            ret = lookup_record_unsafe(ptr);
            if (ret)
            {
                if ((required = strlen(ret) + 1) < *out_size)
                {
                    *out_size = required;
                    ret_out = 1;
                }
                else
                {
                    strncpy(out, ret, *out_size);
                    ret_out = 0;
                }
            }
            else
                ret_out = -1;
            lock_unlock();
            return ret_out;
        }
};

#endif /* #ifdef _LEAKS_SYMCACHE_H */
