#ifndef _LEAKS_SYMCACHE_CC
#define _LEAKS_SYMCACHE_CC
#include "leak_symcache.h"
#include <string.h>


Symcache::Symcache()
{
    initialized = 0;

    lock_init();

    n_buckets = DEFAULT_BUCKETS;

    buckets = (snode_t*)calloc(n_buckets * sizeof(snode_t), 1);
    if (!buckets)
        return;

    lruhead.next = &lrutail;
    lruhead.prev = NULL;
    lrutail.prev = &lruhead;
    lrutail.next = NULL;

    max_records = DEFAULT_MAX_RECORDS;

    n_entries = 0;

    initialized = 1;
}

int Symcache::remove_record_unsafe(void* ptr)
{
    size_t              thebucket       = 0;
    snode_ptr_t         prev            = 0;
    symrecord_ptr_t     thenode         = 0;

    thebucket = hash(ptr);    
    prev = &buckets[thebucket];
    while (prev->next && ((symrecord_ptr_t)(prev->next))->ptr != ptr)
        prev = prev->next;
    if (!prev->next)
        return -1;
    thenode = (symrecord_ptr_t)prev->next;
    prev->next = prev->next->next;

    // Now remove it from the lru
    thenode->lru.next->prev = thenode->lru.prev;
    thenode->lru.prev->next = thenode->lru.next;

    free(thenode);
    --n_entries;

    return 0;
}

char* Symcache::lookup_record_unsafe(void* ptr)
{
    size_t              thebucket       = 0;
    snode_ptr_t         prev            = 0;

    thebucket = hash(ptr);
    prev = &buckets[thebucket];

    while (prev->next != NULL &&
            ((symrecord_ptr_t)(prev->next))->ptr < ptr)
        prev = prev->next;

    if (prev->next &&
            ((symrecord_ptr_t)(prev->next))->ptr == ptr)
    {
        // TODO: move to the head of the lru list
        symrecord_ptr_t therecord = (symrecord_ptr_t)(prev->next);
        therecord->lru.next->prev = therecord->lru.prev;
        therecord->lru.prev->next = therecord->lru.next;
        
        therecord->lru.next = lruhead.next;
        therecord->lru.prev = &lruhead;
        therecord->lru.next->prev = &therecord->lru;
        therecord->lru.prev->next = &therecord->lru;

        return therecord->symbol;
    }

    return NULL;
}


int Symcache::insert_record_unsafe(void* ptr, const char* symbol)
{
    symrecord_ptr_t         record      = 0;
    symrecord_ptr_t         rec_to_rem  = 0;
    size_t                  symlen      = 0;
    size_t                  thebucket   = 0;
    snode_ptr_t             prev        = 0;

    if (!symbol)
        return -1;

    symlen = strlen(symbol);
    if (!symlen)
        return -1;

    record = (symrecord_ptr_t)calloc(sizeof(symrecord_t) + symlen + 1, 1);
    if (!record)
        return -1;

    record->ptr = ptr;
    strcpy(record->symbol, symbol);

    // Insert into the bucket, sorted
    thebucket = hash(ptr);
    prev = &buckets[thebucket];
    while (NULL != prev->next &&
            (size_t)(((symrecord_ptr_t)(prev->next))->ptr) < (size_t)ptr)
        prev = prev->next;
    if (prev->next && ((symrecord_ptr_t)(prev->next))->ptr == ptr)
    {
        free(record);
        return 0;
    }
    record->hashnext.next = prev->next;
    prev->next = &record->hashnext;

    // Now add it to the top of the lru queue
    record->lru.next = lruhead.next;
    record->lru.prev = &lruhead;
    record->lru.prev->next = &record->lru;
    record->lru.next->prev = &record->lru;

    ++n_entries;
    
    if (n_entries > max_records)
    {
        rec_to_rem = (symrecord_ptr_t)
                ((size_t)lrutail.prev - OFFSETOF(symrecord_t, lru));
        remove_record_unsafe(rec_to_rem->ptr);
    }

    return 0;
}
#endif /* #ifdef _LEAKS_SYMCACHE_CC */
