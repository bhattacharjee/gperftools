#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FALSE 0
#define TRUE 1

void (*tc_unmonitor_monitor_leaks)();
int start_monitoring(char *filename)
{
    int (*tc_monitor_leaks)(char *filename);

    tc_monitor_leaks = dlsym(NULL, "tc_monitor_leaks");
    if (!tc_monitor_leaks)
    {
        fprintf(stderr, "Could not find symbol tc_monitor_monitor_leaks. "
                "Check if LD_PRELOAD=./.libs/libtcmalloc_minimal.so is set.\n");
        return -1;
    }

    return tc_monitor_leaks(filename);
}

void stop_monitoring()
{
    void (*tc_unmonitor_monitor_leaks)();
    tc_unmonitor_monitor_leaks = dlsym(NULL, "tc_unmonitor_monitor_leaks");
    if (!tc_unmonitor_monitor_leaks)
    {
        fprintf(stderr, "Could not find symbol tc_unmonitor_monitor_leaks. "
                "Check if LD_PRELOAD=./.libs/libtcmalloc_minimal.so is set.\n");
        return;
    }
    return;
}

int check_count(char* filename, void* ptr, int count)
{
    int     counted     = 0;
    FILE*   fp          = NULL;
    char*   buffer;
    size_t  len;
    char    pointer[32];


    fp = fopen(filename, "r");
    if (!fp)
    {
        fprintf(stderr, "Could not open %s in read mode", filename);
        return FALSE;
    }

    sprintf(pointer, "%p", ptr);

    while (!feof(fp))
    {
        ssize_t readbyt, zeroind;
        len = 0;
        buffer = 0;
        readbyt = getline(&buffer, &len, fp);
        {
            zeroind = (readbyt + 1 < sizeof(buffer) / sizeof(char)) ?
                                readbyt + 1 : sizeof(buffer) / sizeof(char) - 1;
            buffer[zeroind] = 0;
            if (strstr(buffer, pointer))
                counted++;
        }
        if (buffer)
            free(buffer);
    }

    if (counted == count)
        return TRUE;
    return FALSE;
}

int make_readable(char* filename)
{
    return ! chmod(filename, 0777);
}

void main()
{
    void *p1, *p2, *p3, *p4;
    char *filename = "/tmp/filename.txt";

    p1 = malloc(5);
    if (!p1)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }

    if (!start_monitoring(filename))
    {
        fprintf(stderr, "Could not start monitoring\n");
        exit(1);
    }

    p2 = malloc(5);
    if (!p2)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }
    p3 = malloc(5);
    if (!p3)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }

    free(p2);
    stop_monitoring();
    sleep(1); // Give it some time to flush

    p4 = malloc(5);
    if (!p4)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }

    sleep(1); // Give it some more time

    if (!make_readable(filename))
    {
        fprintf(stderr, "Filaneme %s is not readable\n", filename);
        exit(1);
    }
    if (FALSE == check_count(filename, p1, 1))
    {
        fprintf(stderr, "%p expected once, unmatched\n", p1);
    }
    if (FALSE == check_count(filename, p2, 2))
    {
        fprintf(stderr, "%p expected twice, unmatched\n", p2);
    }
    if (FALSE == check_count(filename, p3, 2))
    {
        fprintf(stderr, "%p expected twice, unmatched\n", p3);
    }
    if (FALSE == check_count(filename, p4, 0))
    {
        fprintf(stderr, "%p expected zero times, unmatched\n", p3);
    }
    exit(0);
}
