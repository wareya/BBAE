#ifndef _INCLUDE_BXX_SORTING
#define _INCLUDE_BXX_SORTING

#include <cstddef> // size_t
#include <utility> // std::move, std::swap

// T must be a movable type.

// ordering:
// insertion sort
// helpers
// merge sort (in-place)
// merge sort (temp buffer)
// quicksort

// ####
// helpers
// ####

template<typename T>
int get_default_insertion_sort_size()
{
    if constexpr (std::is_trivially_copyable<T>::value)
        return 32;
    else
        return 8;
}

template<typename T, typename D>
void transfer_into_uninit(D dst, D src, size_t count)
{
    if constexpr (std::is_trivially_copyable<T>::value)
        memcpy(dst, src, sizeof(T) * count);
    else
    {
        for (ptrdiff_t i = count - 1; i >= 0; i -= 1)
        {
            ::new((void*)(dst + i)) T(std::move(src[i]));
            src[i].~T();
        }
    }
}
template<typename Comparator>
void bsearch(size_t & a, size_t b, int round_up, int b_offs, Comparator f)
{
    // rounding-and-offset-aware binary search
    // (doesn't care whether a or b is higher)
    while (a != b)
    {
        size_t avg = (a + b + round_up) / 2;
        if (f(avg)) a = avg;
        else        b = avg + b_offs;
    }
}

template<typename T, typename D, typename Comparator>
void shrink_sort_bounds(Comparator f, D data, size_t & start, size_t & one_past_end)
{
    // shrink bounds to 0 if already sorted
    
    // check if definitely not sorted (or skip out if so small that it doesn't matter)
    if (one_past_end - start < 4 ||
        f(data[one_past_end - 1], data[start]) ||
        f(data[one_past_end - 2], data[start]) ||
        f(data[one_past_end - 1], data[start + 1]))
        return;
    
    // move start right as long as it's not decreasing
    size_t start_p = start;
    while (start_p < one_past_end - 1 && !f(data[start_p + 1], data[start_p]))
        start_p += 1;
    if (start_p == one_past_end - 1) // already sorted
    {
        start = one_past_end - 1;
        return;
    }
}

template<typename T, typename D>
void swap_subsection(D data, size_t start, size_t half_length)
{
    for (size_t i = start; i < start + half_length; i++)
        std::swap(data[i], data[i + half_length]);
}
template<typename T, typename D>
void reverse_subsection(D data, size_t start, size_t length)
{
    for (size_t i = start; i < start + length / 2; i++)
        std::swap(data[i], data[start + length - (i - start) - 1]);
}
template<typename T, typename D>
void rotate_subsection(D data, size_t start, size_t length, size_t left_rotation)
{
    reverse_subsection(data, start, length);
    reverse_subsection(data, start + (length - left_rotation), left_rotation);
    reverse_subsection(data, start, length - left_rotation);
}

// ####
// insertion sort
// ####

#include <assert.h>

template<typename T, typename D, typename Comparator>
void insertion_sort_impl(Comparator f, D data, size_t start, size_t one_past_end)
{
    // hybrid linear-binary insertion sort. does linear until the sorted chunk is a certain size.
    size_t count = one_past_end - start - 1;
    
    size_t count_2;
    if constexpr (std::is_trivially_copyable<T>::value)
        count_2 = 128;
    else
        count_2 = 0;
    
    if (count_2 > count)
        count_2 = count;
    size_t cutoff = one_past_end - 2 - count_2;
    count -= count_2;
    
    // linear part
    for (size_t i = one_past_end - 2; count_2 > 0; i -= 1)
    {
        count_2 -= 1;
        if (f(data[i + 1], data[i]))
        {
            T item(std::move(data[i]));
            size_t j;
            for(j = i; j < one_past_end - 1 && f(data[j + 1], item); j += 1)
                data[j] = std::move(data[j + 1]);
            data[j] = std::move(item);
        }
    }
    
    // binary part
    for (size_t i = cutoff; count > 0; i -= 1)
    {
        count -= 1;
        if (f(data[i + 1], data[i]))
        {
            T item(std::move(data[i]));
            
            size_t insert_i = i + 1;
            bsearch(insert_i, one_past_end - 1, 1, -1, [&](auto avg)
                { return f(data[avg], item); });
            
            if constexpr (std::is_trivially_copyable<T>::value)
            {
                memmove(&data[i], &data[i + 1], sizeof(T) * (insert_i - i));
                data[insert_i] = std::move(item);
            }
            else
            {
                size_t j;
                for (j = i; j < insert_i; j += 1)
                    data[j] = std::move(data[j + 1]);
                data[j] = std::move(item);
            }
        }
    }
}

template<typename T, typename D, typename Comparator>
void tree_insertion_sort_impl(Comparator f, D & data, size_t start, size_t one_past_end)
{
    for (size_t i = start + 1; i < one_past_end; i += 1)
    {
        if (f(data[i], data[i - 1]))
        {
            T item(data.erase_at(i));
            
            size_t insert_i = i - 1;
            bsearch(insert_i, start, 0, 1, [&](auto avg)
            //size_t insert_i = start;
            //bsearch(insert_i, i - 1, 1, -1, [&](auto avg)
                { return f(item, data[avg]); });
                //{ return !f(data[avg], item); });
            
            data.insert_at(insert_i, item);
        }
    }
}

// ####
// merge sort (temp buffer)
// ####

template<bool half_may_be_zero_size, typename T, typename D, typename Comparator>
void merge_parts(Comparator f, D data, D temp_buffer, size_t start, size_t one_past_end, size_t midpoint, char color)
{
    D read_buf  = color ? data : temp_buffer;
    D write_buf = color ? temp_buffer : data;
    
    if (half_may_be_zero_size && (one_past_end == midpoint || midpoint == start))
    {
        transfer_into_uninit<T>(write_buf + start, read_buf + start, one_past_end - start);
        return;
    }
    
    // skip any pre-sorted prefixes, while ensuring that we swap into the other buffer
    // (suffixes will be handled as fast fall-through cases and don't need to be explicly skipped)
    
    // binary search to find length of prefix
    if (midpoint > start && !f(read_buf[midpoint], read_buf[start]))
    {
        size_t start_lo = start + 1;
        bsearch(start_lo, midpoint - 1, 1, -1, [&](auto avg)
            { return !f(read_buf[midpoint], read_buf[avg]); });
        
        transfer_into_uninit<T>(write_buf + start, read_buf + start, start_lo - start);
        start = start_lo;
    }
    
    size_t j = midpoint;
    size_t k = start;
    
    // directly copy any high right tail of the right half
    if (one_past_end > j && !f(read_buf[one_past_end - 1], read_buf[midpoint - 1]))
    {
        size_t end_lo = one_past_end - 1;
        bsearch(end_lo, j, 0, 1, [&](auto avg)
            { return !f(read_buf[avg], read_buf[midpoint - 1]); });
        
        transfer_into_uninit<T>(write_buf + end_lo, read_buf + end_lo, one_past_end - end_lo);
        one_past_end = end_lo;
    }
    
    size_t j_hi = one_past_end;
    
    // merge from read buffer into write buffer by checking whether the bottom unmerged value of which subarray is lower
    size_t i = start;
    while (i < midpoint && j < j_hi)
    {
        size_t index;
        if (f(read_buf[j], read_buf[i]))
            index = j++;
        else
            index = i++;
        
        if constexpr (std::is_trivially_copyable<T>::value)
            write_buf[k++] = read_buf[index];
        else
        {
            ::new((void*)(write_buf + (k++))) T(std::move(read_buf[index]));
            read_buf[index].~T();
        }
    }
    if (j < j_hi)
        transfer_into_uninit<T>(write_buf + k, read_buf + j, one_past_end - k);
    else
        transfer_into_uninit<T>(write_buf + k, read_buf + i, one_past_end - k);
}

template<typename T, typename D, typename Comparator>
void merge_sort_bottomup_impl(Comparator f, D data, D temp_buffer, size_t start, size_t one_past_end)
{
    size_t size = one_past_end - start;
    size_t block_size = get_default_insertion_sort_size<T>();
    
    // pick an insertion sort size that guarantees that we won't need to swap the buffers an extra time at the end
    size_t temp = block_size;
    size_t tiers = 0;
    while (temp < size)
    {
        temp <<= 1;
        tiers += 1;
    }
    if (tiers & 1)
    {
        block_size >>= 1;
        tiers += 1;
    }
    
    // divide array up into small chunks; use insertion sort on them
    size_t blocks = 0;
    size_t i;
    for (i = start; i < one_past_end; i += block_size)
    {
        size_t cap = i + block_size;
        if (cap > one_past_end)
            cap = one_past_end;
        insertion_sort_impl<T>(f, data, i, cap);
        
        // merge partial results into leftwards block as possible + while we go
        // this is more cache-coherent than the textbook "one layer at a time" bottom-up merge algorithm
        size_t blocks_n = blocks;
        size_t width = block_size;
        size_t i2 = i;
        D buf_a = data;
        D buf_b = temp_buffer;
        while (blocks_n & 1)
        {
            blocks_n >>= 1;
            i2 -= width;
            merge_parts<false, T>(f, buf_a, buf_b, i2, cap, i2 + width, 1);
            width <<= 1;
            std::swap(buf_a, buf_b);
        }
        blocks += 1;
    }
    // finish merging in the rightmost edge if our block count wasn't a power of 2
    if (blocks != (1U << tiers))
    {
        i -= block_size;
        D buf_a = data;
        D buf_b = temp_buffer;
        blocks -= 1;
        
        // skip already-done merge work
        while (blocks & 1)
        {
            blocks >>= 1;
            i -= block_size;
            block_size <<= 1;
            std::swap(buf_a, buf_b);
        }
        D prev_write_buf = buf_a;
        
        // finish up rest of merge work
        while (blocks > 0)
        {
            if (blocks & 1)
            {
                // flip into correct buffer if needed
                for (size_t j = i; prev_write_buf != buf_a && j < one_past_end; j++)
                {
                    ::new((void*)(buf_a + j)) T(std::move(buf_b[j]));
                    buf_b[j].~T();
                }
                i -= block_size;
                merge_parts<false, T>(f, buf_a, buf_b, i, one_past_end, i + block_size, 1);
                prev_write_buf = buf_b;
            }
            block_size <<= 1;
            blocks >>= 1;
            std::swap(buf_a, buf_b);
        }
    }
}

// ####
// quick sort
// ####
template<typename T, typename D, typename Comparator>
void quick_sort_impl(Comparator f, D data, size_t start, size_t one_past_end)
{
    size_t insert_sort_size = get_default_insertion_sort_size<T>();
    
    // if small, use insertion sort (faster on small arrays for cache reasons)
    if (one_past_end - start <= insert_sort_size)
    {
        if (one_past_end - start >= 2)
            insertion_sort_impl<T>(f, data, start, one_past_end);
        return;
    }
    
    // start with middle position as pivot for the sake of simplicity
    size_t pivot_lo = start;
    size_t pivot_hi = one_past_end - 1;
    size_t pivot = pivot_lo + (pivot_hi - pivot_lo) / 2;
    // perturb pivot position arbitrarily based on a few comparisons
    // (this is meant to deterministically emulate an RNG)
    char inv = 1;
    for (size_t i = 0; i < 5 && pivot_lo != pivot_hi; i++)
    {
        if (inv == !f(data[pivot], data[pivot + 1]))
            pivot_hi = pivot;
        else
            pivot_lo = pivot + 1;
        pivot = pivot_lo + (pivot_hi - pivot_lo) / 2;
        inv = !inv;
    }
    
    // now perform partitioning
    // partition array into two parts separated by the pivot value
    size_t i = start;
    size_t j = one_past_end - 1;
    while (i < j)
    {
        while (f(data[i], data[pivot])) i += 1;
        while (f(data[pivot], data[j])) j -= 1;
        
        if (i < j)
            std::swap(data[i++], data[j--]);
    }
    
    quick_sort_impl<T>(f, data, start, i);
    if (i + 1 < one_past_end)
        quick_sort_impl<T>(f, data, i + 1, one_past_end);
}

#endif // _INCLUDE_BXX_SORTING
