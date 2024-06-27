#include <stdint.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

uint8_t * copy_as_executable(uint8_t * mem, size_t len)
{
    void * const buffer = VirtualAlloc(0, len, MEM_COMMIT, PAGE_READWRITE);
    
    printf("0x%zX\n", (uint64_t)buffer);
    
    memcpy(buffer, mem, len);
    DWORD dummy;
    assert(VirtualProtect(buffer, len, PAGE_EXECUTE_READ, &dummy));
    
    puts("-- marked page as executable");
    
    return (uint8_t *)buffer;
}

void free_as_executable(uint8_t * mem)
{
    VirtualFree(mem, 0, MEM_RELEASE);
}

#else

#include <sys/mman.h>
#include <unistd.h>

size_t jit_alloc_size = 0;
uint8_t * copy_as_executable(uint8_t * mem, size_t len)
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

void free_as_executable(uint8_t * mem)
{
    munmap(mem, jit_alloc_size);
}

#endif
