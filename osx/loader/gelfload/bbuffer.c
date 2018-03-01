#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../config.h"

#include "bbuffer.h"

#if defined(HAVE_MMAP)
#include <sys/mman.h>

/* mmap-based is the easiest */
void *bbuffer(void *loc, size_t sz) {
    void *ret;
    int page = getpagesize();

    /* switch on fixed-ness */
    if (loc) {
        /* make sure it's on a page boundary */
        void *wantloc = loc;
        ssize_t offset;
        offset = (ssize_t) loc % page;
        if (offset) {
            sz += offset;
            loc = (char *)loc - offset;
        }

        ret = mmap(loc, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_FIXED,
                   -1, 0);
        if (ret == loc) ret = wantloc;
    } else {
        ret = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON,
                   -1, 0);
    }

    if (ret == (void *) -1) {
        perror("mmap");
        exit(1);
    }

    return ret;
}

int xbbuffer(void *loc, size_t sz)
{
    return mprotect(loc, sz, PROT_READ | PROT_EXEC);
}

void unbbuffer(void *loc, size_t sz)
{
    munmap(loc, sz);
}

#elif defined(__WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

void *bbuffer(void *loc, size_t sz)
{
    /* shockingly, Windows makes this far more difficult than it needs to be */
    SYSTEM_INFO si;
    MEMORY_BASIC_INFORMATION mbi;
    ssize_t agrn, offset, szoffset;
    void *q, *ret;

    if (loc) {
        /* the complicated case */

        /* get the allocation granularity */
        GetSystemInfo(&si);
        agrn = si.dwAllocationGranularity;

        /* make sure it's on the allocation granularity */
        offset = (ssize_t) loc % agrn;
        if (offset) {
            sz += offset;
            loc -= offset;
        }

        /* then make sure sz is also aligned */
        szoffset = sz % agrn;
        if (szoffset) {
            sz += (agrn - szoffset);
        }

        /* free everything */
        for (q = loc; q < loc + sz; q += agrn) {
            if (VirtualQuery(q, &mbi, agrn) >= sizeof(mbi)) {
                if (mbi.State != MEM_FREE) {
                    /* get rid of it */
                    if (mbi.Type & MEM_IMAGE)
                        UnmapViewOfFile(mbi.AllocationBase);
                    VirtualFree(mbi.AllocationBase, 0, MEM_RELEASE);
                }
            }
        }

        /* finally, allocate */
        ret = VirtualAlloc(loc, sz, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        if (ret == NULL) {
            fprintf(stderr, "VirtualAlloc failed with error %d\n", GetLastError());
            exit(1);
        }

        return ret + offset;

    } else {
        ret = VirtualAlloc(NULL, sz, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        if (ret == NULL) {
            fprintf(stderr, "VirtualAlloc failed with error %d\n", GetLastError());
            exit(1);
        }

        return ret;

    }
}

void unbbuffer(void *loc, size_t sz)
{
    VirtualFree(loc, 0, MEM_RELEASE);
}

int xbbuffer(void *loc, size_t sz)
{
    DWORD old;
    return !VirtualProtect(loc, sz, PAGE_EXECUTE_READ, &old);
}

#else

void *bbuffer(void *loc, size_t sz)
{
    /* worst case scenario, just malloc */
    void *ret = malloc(sz);
    if (ret == NULL) {
        perror("malloc");
        exit(1);
    }
    return ret;
}

int xbbuffer(void *loc, size_t sz)
{
    return -1;
}

void unbbuffer(void *loc, size_t sz)
{
    free(loc);
}

#endif