#ifndef _BBAE_MEMORY_H
#define _BBAE_MEMORY_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ALLOC_PREFIX_SIZE ((uint64_t)64)

uint64_t * alloc_get_size(uint8_t * alloc)
{
     return ((uint64_t *)alloc) + 2;
}
uint8_t ** alloc_get_prev_ptr(uint8_t * alloc)
{
     return ((uint8_t **)alloc) + 1;
}
uint8_t ** alloc_get_next_ptr(uint8_t * alloc)
{
     return (uint8_t **)alloc;
}

uint8_t * alloc_base_loc(void * buf)
{
    return ((uint8_t *)buf) - ALLOC_PREFIX_SIZE;
}
uint8_t * alloc_data_loc(void * alloc)
{
    return ((uint8_t *)alloc) + ALLOC_PREFIX_SIZE;
}

uint8_t * alloc_list = 0;
void * zero_alloc(size_t n)
{
    n += ALLOC_PREFIX_SIZE;
    
    uint8_t * alloc = (uint8_t *)realloc(0, n);
    assert(alloc);
    memset(alloc, 0, n);
    
    *alloc_get_next_ptr(alloc) = alloc_list;
    if (alloc_list)
        *alloc_get_prev_ptr(alloc_list) = alloc;
    
    *alloc_get_size(alloc) = n - ALLOC_PREFIX_SIZE;
    
    alloc_list = alloc;
    
    return alloc_data_loc(alloc);
}
void * zero_realloc(uint8_t * buf, size_t n)
{
    n += ALLOC_PREFIX_SIZE;
    
    uint8_t * new_alloc = (uint8_t *)realloc(alloc_base_loc(buf), n);
    assert(new_alloc);
    
    uint64_t prev_n = (*alloc_get_size(new_alloc)) + ALLOC_PREFIX_SIZE;
    if (n > prev_n)
        memset(new_alloc + prev_n, 0, n - prev_n);
    
    *alloc_get_size(new_alloc) = n - ALLOC_PREFIX_SIZE;
    
    uint8_t * prev_alloc = *alloc_get_prev_ptr(new_alloc);
    if (prev_alloc)
        *alloc_get_next_ptr(prev_alloc) = new_alloc;
    uint8_t * next_alloc = *alloc_get_next_ptr(new_alloc);
    if (next_alloc)
        *alloc_get_prev_ptr(next_alloc) = new_alloc;
    
    return alloc_data_loc(new_alloc);
}
void free_all_compiler_allocs(void)
{
    while (alloc_list)
    {
        uint8_t * alloc_next = *(uint8_t **)alloc_list;
        free(alloc_list);
        alloc_list = alloc_next;
    }
}

#define array_len(ARRAY, TYPE) \
    ((*alloc_get_size(alloc_base_loc((uint8_t *)(ARRAY)))) / sizeof(TYPE))

#define array_push(ARRAY, TYPE, VAL) \
    (ARRAY = ((TYPE *)zero_realloc((uint8_t *)(ARRAY), *alloc_get_size(alloc_base_loc((uint8_t *)(ARRAY))) + sizeof(TYPE))), \
     ((TYPE *)(ARRAY))[array_len((uint8_t *)(ARRAY), TYPE) - 1] = (VAL) \
    )


// Returns a pointer to a buffer with len+1 bytes reserved, and at least one null terminator.
char * strcpy_len(const char * str, size_t len)
{
    if (!str)
        return (char *)zero_alloc(1);
    char * ret = (char *)zero_alloc(len + 1);
    assert(ret);
    char * ret_copy = ret;
    char c = 0;
    while ((c = *str++) && len-- > 0)
        *ret_copy++ = c;
    return ret;
}
char * strcpy_z(const char * str)
{
    assert(str);
    size_t len = strlen(str);
    return strcpy_len(str, len);
}

uint8_t str_ends_with(const char * a, const char * b)
{
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    if (len_a < len_b)
        return 0;
    if (len_b == 0)
        return 1;
    
    a += len_a - len_b;
    return strncmp(a, b, len_b) == 0;
}
uint8_t str_begins_with(const char * a, const char * b)
{
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    if (len_a < len_b)
        return 0;
    if (len_b == 0)
        return 1;
    
    return strncmp(a, b, len_b) == 0;
}
uint8_t is_space(char c)
{
    return c == ' ' || c == '\t';
}
void skip_space(const char ** b)
{
    while (is_space(**b))
        *b += 1;
}
void to_space(const char ** b)
{
    while (!is_space(**b))
        *b += 1;
}
uint8_t is_newline(char c)
{
    return c == '\r' || c == '\n';
}
uint8_t is_comment(const char * b)
{
    if (b == 0 || b[0] == 0)
        return 0;
    if (b[0] == '/' && b[1] == '/')
        return 1;
    if (b[0] == '#')
        return 1;
    return 0;
}

void skip_newline(const char ** b)
{
    while (is_newline(**b))
        *b += 1;
}
void to_newline(const char ** b)
{
    while (!is_newline(**b))
        *b += 1;
}

#endif // _BBAE_MEMORY_H
