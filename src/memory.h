#ifndef _BBAE_MEMORY_H
#define _BBAE_MEMORY_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ALLOC_PREFIX_SIZE ((uint64_t)64)

static uint64_t * alloc_get_size(uint8_t * alloc)
{
     return ((uint64_t *)alloc) + 2;
}
static uint8_t ** alloc_get_prev_ptr(uint8_t * alloc)
{
     return ((uint8_t **)alloc) + 1;
}
static uint8_t ** alloc_get_next_ptr(uint8_t * alloc)
{
     return (uint8_t **)alloc;
}

static uint8_t * alloc_base_loc(void * buf)
{
    return ((uint8_t *)buf) - ALLOC_PREFIX_SIZE;
}
static uint8_t * alloc_data_loc(void * alloc)
{
    return ((uint8_t *)alloc) + ALLOC_PREFIX_SIZE;
}

static uint8_t * alloc_list = 0;
static void * zero_alloc(size_t n)
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
static void * zero_realloc(uint8_t * buf, size_t n)
{
    n += ALLOC_PREFIX_SIZE;
    
    uint8_t * old_alloc = alloc_base_loc(buf);
    uint8_t * new_alloc = (uint8_t *)realloc(old_alloc, n);
    assert(new_alloc);
    
    if (alloc_list == old_alloc)
        alloc_list = new_alloc;
    
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
static void free_all_compiler_allocs(void)
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

static void array_erase_impl(uint8_t ** array, size_t item_size, size_t index)
{
    size_t size = *alloc_get_size(alloc_base_loc(*array));
    size_t start = item_size * index;
    memmove(*array + start, *array + start + item_size, size - start - item_size);
    
    *array = zero_realloc(*array, size - item_size);
}
#define array_erase(ARRAY, TYPE, INDEX) \
    (array_erase_impl((uint8_t **)(&(ARRAY)), sizeof(TYPE), INDEX))

static void array_insert_impl(uint8_t ** array, size_t item_size, size_t index)
{
    size_t size = *alloc_get_size(alloc_base_loc(*array)) + item_size;
    *array = zero_realloc(*array, size);
    
    size_t start = item_size * index;
    memmove(*array + start + item_size, *array + start, size - start - item_size);
}

#define array_insert(ARRAY, TYPE, INDEX, VAL) \
    (array_insert_impl((uint8_t **)(&(ARRAY)), sizeof(TYPE), (INDEX)), \
     ((TYPE *)(ARRAY))[(INDEX)] = (VAL) \
    )

static size_t ptr_array_find_impl(void ** array, void * ptr)
{
    for (size_t i = 0; i < array_len(array, void *); i++)
    {
        if (array[i] == ptr)
            return (ptrdiff_t)i;
    }
    return -1;
}

#define ptr_array_find(ARRAY, VAL) (ptr_array_find_impl((void**)(ARRAY), (VAL)))

static size_t array_find_impl(void * array, size_t item_size, void * ptr)
{
    for (size_t i = 0; i < array_len(array, void *); i++)
    {
        if (memcmp((uint8_t *)array + i*item_size, ptr, item_size) == 0)
            return (ptrdiff_t)i;
    }
    return -1;
}

#define array_find(ARRAY, TYPE, VAL) (array_find_impl((void*)(ARRAY), sizeof(TYPE), &(VAL)))

static uint8_t * array_chop_impl(uint8_t ** array, size_t start_offs, size_t end_offs)
{
    uint8_t * ret = (uint8_t *)zero_alloc(end_offs - start_offs);
    memcpy(ret, *array + start_offs, end_offs - start_offs);
    *array = zero_realloc(*array, start_offs);
    return ret;
}

#define array_chop(ARRAY, TYPE, I) ((TYPE*)array_chop_impl((uint8_t**)(&(ARRAY)), sizeof(TYPE) * (I), array_len(ARRAY, TYPE) * (I)))

// Returns a pointer to a buffer with len+1 bytes reserved, and at least one null terminator.
static char * strcpy_len(const char * str, size_t len)
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
static char * strcpy_z(const char * str)
{
    assert(str);
    size_t len = strlen(str);
    return strcpy_len(str, len);
}

static uint8_t str_ends_with(const char * a, const char * b)
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
static uint8_t str_begins_with(const char * a, const char * b)
{
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    if (len_a < len_b)
        return 0;
    if (len_b == 0)
        return 1;
    
    return strncmp(a, b, len_b) == 0;
}
static uint8_t is_space(char c)
{
    return c == ' ' || c == '\t';
}
static void skip_space(const char ** b)
{
    while (is_space(**b))
        *b += 1;
}
static void to_space(const char ** b)
{
    while (!is_space(**b))
        *b += 1;
}
static uint8_t is_newline(char c)
{
    return c == '\r' || c == '\n';
}
static uint8_t is_comment(const char * b)
{
    if (b == 0 || b[0] == 0)
        return 0;
    if (b[0] == '/' && b[1] == '/')
        return 1;
    if (b[0] == '#')
        return 1;
    return 0;
}

static void skip_newline(const char ** b)
{
    while (is_newline(**b))
        *b += 1;
}
static void to_newline(const char ** b)
{
    while (!is_newline(**b))
        *b += 1;
}

#endif // _BBAE_MEMORY_H
