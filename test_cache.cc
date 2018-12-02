#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST(X, Y) if(!(X)){fprintf(stderr, "%d: %s\n", __LINE__,(Y)); exit(1);}
#define _V(X) (void*)(X)
#define _S(x) n[(x)]

#define DEFAULT_MAX_RECORDS 4

#include "src/leak_symcache.h"
#include "src/leak_symcache.cc"

char* n[] = {
    "999999990",
    "999999991",
    "999999992",
    "999999993",
    "999999994",
    "999999995",
    "999999996",
    "999999997",
    "999999998"
};

Symcache symcache;

int main()
{
    char    buffer[128];
    size_t  bufsiz;

    // Test that insert succeeds
    TEST((0 == symcache.insert_record(_V(1), _S(1))),
            "Could not insert record");

    // Test that we get the same number that we inserted
    TEST((0 <= symcache.lookup_record(_V(1), 0, 0)),
            "Could not find the number we inserted");

    // Test that we get the correct size if we specify something less
    bufsiz = -1;
    TEST((0 <= symcache.lookup_record(_V(1), buffer, &bufsiz)),
            "Could not find the number we inserted");
    TEST((strlen(_S(1)) + 1 == bufsiz), "Buffer size not populated correctly");

    // Test that we are getting the correct buffer when we look for it
    TEST((0 == symcache.lookup_record(_V(1), buffer, &bufsiz)),
            "Could not find the number we inserted");
    TEST((0 == strcmp(buffer, _S(1))),
            "Could not matche stored symbol");

    // Test whether deleting invalid items fails
    TEST((-1 == symcache.remove_record(_V(2))),
            "Should not be able to delete invalid record");

    // Test whether we succeeded in deleting an existing record
    TEST((0 == symcache.remove_record(_V(1))),
            "Should be able to delete valid record");
    TEST((0 > symcache.lookup_record(_V(1), buffer, &bufsiz)),
            "Should not be able to lookup removed record");

    // Check whether we are evicting records or not
    TEST((0 == symcache.insert_record(_V(1), _S(1))),
            "Could not insert record");
    TEST((0 == symcache.insert_record(_V(2), _S(2))),
            "Could not insert record");
    TEST((0 == symcache.insert_record(_V(3), _S(3))),
            "Could not insert record");
    TEST((0 == symcache.insert_record(_V(4), _S(4))),
            "Could not insert record");
    TEST((0 == symcache.insert_record(_V(5), _S(5))),
            "Could not insert record");
    TEST((0 > symcache.lookup_record(_V(1), buffer, &bufsiz)),
            "Old records should be evicted");
    TEST((0 <= symcache.lookup_record(_V(2), buffer, &bufsiz)),
            "new records should not be evicted while evicting old entries");
    TEST((0 == symcache.remove_record(_V(2))),
            "Should be able to delete valid record");
    TEST((0 == symcache.remove_record(_V(3))),
            "Should be able to delete valid record");
    TEST((0 == symcache.remove_record(_V(4))),
            "Should be able to delete valid record");
    TEST((0 == symcache.remove_record(_V(5))),
            "Should be able to delete valid record");

    // Check if LRU process is being followed or not
    TEST((0 == symcache.insert_record(_V(1), _S(1))),
            "Could not insert record");
    TEST((0 == symcache.insert_record(_V(2), _S(2))),
            "Could not insert record");
    TEST((0 == symcache.insert_record(_V(3), _S(3))),
            "Could not insert record");
    TEST((0 == symcache.insert_record(_V(4), _S(4))),
            "Could not insert record");
    TEST((0 == symcache.lookup_record(_V(1), buffer, &bufsiz)),
            "Could not find the number we inserted");
    TEST((0 == symcache.insert_record(_V(5), _S(5))),
            "Could not insert record");
    TEST((0 == symcache.lookup_record(_V(1), buffer, &bufsiz)),
            "Recently used numbers should not be evicted");
    TEST((0 == symcache.lookup_record(_V(3), buffer, &bufsiz)),
            "Recently used numbers should not be evicted");
    TEST((0 == symcache.lookup_record(_V(4), buffer, &bufsiz)),
            "Recently used numbers should not be evicted");
    TEST((0 == symcache.lookup_record(_V(5), buffer, &bufsiz)),
            "Recently used numbers should not be evicted");
    TEST((0 > symcache.lookup_record(_V(2), buffer, &bufsiz)),
            "Least Recently used numbers should be evicted");
    TEST((0 == symcache.remove_record(_V(1))),
            "Should be able to delete valid record");
    TEST((0 == symcache.remove_record(_V(3))),
            "Should be able to delete valid record");
    TEST((0 == symcache.remove_record(_V(4))),
            "Should be able to delete valid record");
    TEST((0 == symcache.remove_record(_V(5))),
            "Should be able to delete valid record");

    // Should not be able to insert an entry for the same key twice
    TEST((0 == symcache.insert_record(_V(1), _S(1))),
            "Could not insert record");
    TEST((0 == symcache.insert_record(_V(1), _S(2))),
            "Could not insert record");
    TEST((0 == symcache.lookup_record(_V(1), buffer, &bufsiz)),
            "Could not find the number we inserted");
    TEST((0 == strcmp(buffer, _S(1))),
            "Could not matche stored symbol");
}

