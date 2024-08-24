#ifndef JITIFY_H
#define JITIFY_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

static size_t get_page_size(void)
{
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    return siSysInfo.dwPageSize;
}

static void * VirtualAllocNearProcess(size_t len)
{
    size_t check_size = 1 << 16;
    size_t start_at = (size_t)(void *)VirtualAllocNearProcess;
    start_at = (start_at >> 16) << 16;
    size_t orig_start_at = start_at;
    size_t start_at_2 = start_at;
    len = ((len + (1 << 16)) >> 16) << 16;
    void * buffer = VirtualAlloc((void *)start_at, len, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    while (!buffer && (start_at - start_at_2) < (1ll<<31ll))
    {
        start_at += check_size;
        start_at_2 -= check_size;
        buffer = VirtualAlloc((void *)start_at, len, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        if (buffer)
            return buffer;
        buffer = VirtualAlloc((void *)start_at_2, len, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        if (buffer)
            return buffer;
    }
    if (!buffer)
    {
        printf("ERROR: failed to allocate buffer (tried up to 0x%zX distance)\n", (uint64_t)(start_at - start_at_2)/2);
        assert(0);
    }
    return buffer;
}

/// returns read-write memory near the process address space
static uint8_t * alloc_near_executable(size_t len)
{
    printf("allocating %zu length\n", len);
    // we want an allocation that's near our own process memory
    // so that we can do 32-bit relocations when calling non-JIT functions
    void * const buffer = VirtualAllocNearProcess(len);
    assert(buffer);
    ptrdiff_t diff = ((ptrdiff_t)buffer) - ((ptrdiff_t)VirtualAllocNearProcess);
    printf("%zd\n", diff);
    assert(diff > -(1ll<<30ll) && diff < (1ll<<30ll));
    
    return buffer;
}

/// returns executable memory near the process address space
/// actual size allocated is written into len, which must not be null
static uint8_t * copy_as_executable(uint8_t * mem, size_t * len)
{
    uint8_t * buffer = alloc_near_executable(*len);
    
    memcpy(buffer, mem, *len);
    DWORD dummy;
    assert(VirtualProtect(buffer, *len, PAGE_EXECUTE_READ, &dummy));
    
    return (uint8_t *)buffer;
}

static void free_as_executable(uint8_t * mem, size_t len)
{
    (void)len; // unused
    VirtualFree(mem, 0, MEM_RELEASE);
}

#else // of #ifdef _WIN32

#include <sys/mman.h>
#include <unistd.h>

/// returns executable memory near the process address space
/// actual size allocated is written into len, which must not be null
static uint8_t * copy_as_executable(uint8_t * mem, size_t len)
{
    int pagesize = getpagesize();
    jit_alloc_size = pagesize;
    while (jit_alloc_size < len)
        jit_alloc_size += pagesize;
    
    void * x = mmap(0, jit_alloc_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    assert(x);
    assert((int64_t)x != -1);
    memcpy(x, mem, len);
    mprotect(x, jit_alloc_size, PROT_READ | PROT_EXEC);
    
    return x;
}

static void free_as_executable(uint8_t * mem)
{
    munmap(mem, jit_alloc_size);
}

#endif // if-else over _WIN32

#endif // JITIFY_H
