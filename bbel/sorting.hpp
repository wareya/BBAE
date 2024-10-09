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

template<typename T>
void transfer_into_uninit(T * dst, T * src, size_t count)
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

template<typename T, typename Comparator>
void shrink_sort_bounds(Comparator f, T * data, size_t & start, size_t & one_past_end)
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

template<typename T>
void swap_subsection(T * data, size_t start, size_t half_length)
{
    for (size_t i = start; i < start + half_length; i++)
        std::swap(data[i], data[i + half_length]);
}
template<typename T>
void reverse_subsection(T * data, size_t start, size_t length)
{
    for (size_t i = start; i < start + length / 2; i++)
        std::swap(data[i], data[start + length - (i - start) - 1]);
}
template<typename T>
void rotate_subsection(T * data, size_t start, size_t length, size_t left_rotation)
{
    reverse_subsection(data, start, length);
    reverse_subsection(data, start + (length - left_rotation), left_rotation);
    reverse_subsection(data, start, length - left_rotation);
}

// ####
// insertion sort
// ####

template<typename T, typename Comparator>
void insertion_sort_impl(Comparator f, T * data, size_t start, size_t one_past_end)
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
template<typename T, typename Comparator>
void insertion_sort_binary_impl(Comparator f, T * data, size_t start, size_t one_past_end)
{
    // hybrid linear-binary insertion sort. does linear until the sorted chunk is a certain size.
    size_t count = one_past_end - start - 1;
    for (size_t i = one_past_end - 2; count > 0; i -= 1)
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
template<typename T, typename Comparator>
void insertion_sort_linear_impl(Comparator f, T * data, size_t start, size_t one_past_end)
{
    size_t count = one_past_end - start - 1;
    for (size_t i = one_past_end - 2; count > 0; i -= 1)
    {
        count -= 1;
        if (f(data[i + 1], data[i]))
        {
            T item(std::move(data[i]));
            size_t j;
            for(j = i; j < one_past_end - 1 && f(data[j + 1], item); j += 1)
                data[j] = std::move(data[j + 1]);
            data[j] = std::move(item);
        }
    }
}

// ####
// merge sort (in-place)
// ####

template<typename T, typename Comparator>
void merge_parts_inplace(Comparator f, T * data, size_t start, size_t one_past_end, size_t midpoint)
{
    // intuition: by finding an inner window that we can safely rotate, we can divide the merging operation up
    //   into two smaller merging operations. this rotation takes O(n) time.
    // example:
    //                   
    //            [    xxx       ]     xxxx
    //            [  xxxxx       ] xxxxxxxx
    //         xxx[xxxxxxx       ]xxxxxxxxx
    //     xxxxxxx[xxxxxxx   xxxx]xxxxxxxxx
    //
    //            [           xxx]     xxxx
    //            [         xxxxx] xxxxxxxx
    //         xxx[       xxxxxxx]xxxxxxxxx
    //     xxxxxxx[   xxxxxxxxxxx]xxxxxxxxx
    //
    //                   ||    xxx     xxxx
    //                   ||  xxxxx xxxxxxxx
    //         xxx       ||xxxxxxxxxxxxxxxx
    //     xxxxxxx   xxxx||xxxxxxxxxxxxxxxx
    //
    // we find the inner window by doing a binary search out from the midpoint, and then,
    //    if it hits the side of the merge range, we do another binary search to extend it sideways if possible
    //
    // assuming the window is chosen correctly, the merge is "stable" (retains relative ordering)
    // assuming the size of the window scales linearly with n, the total cost is O(n logn) or maybe O(n (logn)^x)
    // (rotation is an O(n) operation)
    //
    // without the sideways extension step, some inputs (like reversed arrays) find a window size fails to scale with n
    // I have not 100% proven that the sideways extension step is sufficient to ensure O(n logn) operation, but I'm confident!
    //
    // this is similar in spirit to symmerge, but not directly based on it
    
    if (one_past_end == midpoint || midpoint == start)
        return;
    
    size_t insert_sort_size = get_default_insertion_sort_size<T>();
    if (one_past_end - start <= insert_sort_size)
        return insertion_sort_impl(f, data, start, one_past_end);
    
    if (!f(data[midpoint], data[midpoint - 1]))
        return;
    
    /*
    if (f(data[one_past_end - 1], data[start]))
    {
        if (midpoint - start == one_past_end - midpoint)
            swap_subsection(data, start, midpoint - start);
        else
            rotate_subsection(data, start, one_past_end - start, midpoint - start);
        return;
    }
    */
    
    size_t size = 1;
    size_t size_hi = one_past_end - midpoint;
    if (midpoint - start < size_hi)
        size_hi = midpoint - start;
    
    bsearch(size, size_hi, 1, -1, [&](auto avg)
        { return f(data[midpoint + avg - 1], data[midpoint - avg]); });
    
    size_t left = midpoint - size;
    size_t right = midpoint + size;
    
    // in degenerate cases, this isn't good enough and will result in
    //    O(n) stack depth, because of subarray pairs like:
    // 1,2,3,4,5,6,7,8,9 | 0
    //                   ^---- midpoint
    // such subarray pairs can't be generated by merge sort, but CAN be generated by subarray merging!
    // so, we move our left or right side further to the side as long as it would still cause a stable sort
    // we can do this in logn time using a binary search
    
    if (left > start && right == one_past_end)
    {
        bsearch(left, start, 0, 1, [&](auto avg)
            { return f(data[right - 1], data[avg]); });
    }
    if (right < one_past_end && left == start)
    {
        bsearch(right, one_past_end, 1, -1, [&](auto avg)
            { return f(data[avg - 1], data[left]); });
    }
    
    // rotate inner window
    if (left == midpoint - size && right == midpoint + size)
        swap_subsection(data, midpoint - size, size);
    else
        rotate_subsection(data, left, right - left, midpoint - left);
    
    // merge newly-created left and right block pairs
    merge_parts_inplace(f, data, start, midpoint, left);
    merge_parts_inplace(f, data, midpoint, one_past_end, right);
}

template<typename T, typename Comparator>
void merge_sort_inplace_impl(Comparator f, T * data, size_t start, size_t one_past_end)
{
    size_t insert_sort_size = get_default_insertion_sort_size<T>();
    if (one_past_end - start <= insert_sort_size)
        return insertion_sort_impl(f, data, start, one_past_end);
    
    size_t midpoint = start + (one_past_end - start) / 2;
    merge_sort_inplace_impl(f, data, start, midpoint);
    merge_sort_inplace_impl(f, data, midpoint, one_past_end);
    merge_parts_inplace(f, data, start, one_past_end, midpoint);
}

// ####
// merge sort (temp buffer)
// ####

#include <assert.h>

template<bool half_may_be_zero_size, typename T, typename Comparator>
void merge_parts(Comparator f, T * data, T * temp_buffer, size_t start, size_t one_past_end, size_t midpoint, char color)
{
    T * read_buf  = color ? data : temp_buffer;
    T * write_buf = color ? temp_buffer : data;
    
    if (half_may_be_zero_size && (one_past_end == midpoint || midpoint == start))
    {
        transfer_into_uninit(write_buf + start, read_buf + start, one_past_end - start);
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
        
        transfer_into_uninit(write_buf + start, read_buf + start, start_lo - start);
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
        
        transfer_into_uninit(write_buf + end_lo, read_buf + end_lo, one_past_end - end_lo);
        one_past_end = end_lo;
    }
    
    size_t j_hi = one_past_end;
    
    // merge from read buffer into write buffer by checking whether the bottom unmerged value of which subarray is lower
    size_t i = start;
    //while (k < one_past_end)
    //while (i < midpoint)
    while (i < midpoint && j < j_hi)
    //while (j < j_hi)
    {
        //if (j == j_hi)
        //    break;
        size_t index;
        //if (i == midpoint || (j < j_hi && f(read_buf[j], read_buf[i])))
        if (f(read_buf[j], read_buf[i]))
        //if (j < j_hi && f(read_buf[j], read_buf[i]))
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
        transfer_into_uninit(write_buf + k, read_buf + j, one_past_end - k);
    else
        transfer_into_uninit(write_buf + k, read_buf + i, one_past_end - k);
}

template<typename T, typename Comparator>
void merge_sort_impl(Comparator f, T * data, T * temp_buffer, size_t start, size_t one_past_end, size_t midpoint, char color)
{
    // merge sort:
    // 1) sort two subarrays
    // 2) use a stable, O(n logn) (or otherwise quasilinear) algorithm to merge them
    // for the sake of reducing swaps, we use a buffer-swapping approach
    //   by coloring each call based on its recursion depth
    
    // if small, use insertion sort (faster on small arrays for cache reasons)
    size_t insert_sort_size = get_default_insertion_sort_size<T>();
    if (one_past_end - start <= insert_sort_size)
    {
        if (one_past_end - start >= 2)
            insertion_sort_impl(f, data, start, one_past_end);
        
        // this is a root case; if we need to swap into the temp buffer, do so
        size_t i = start;
        while (color && i < one_past_end)
        {
            ::new((void*)(temp_buffer + i)) T(std::move(data[i]));
            data[i++].~T();
        }
        return;
    }
    
    merge_sort_impl(f, data, temp_buffer, start, midpoint, start + (midpoint - start) / 2, !color);
    merge_sort_impl(f, data, temp_buffer, midpoint, one_past_end, midpoint + (one_past_end - midpoint) / 2, !color);
    merge_parts<false>(f, data, temp_buffer, start, one_past_end, midpoint, color);
}

template<typename T, typename Comparator>
void merge_sort_bottomup_textbook_impl(Comparator f, T * data, T * temp_buffer, size_t start, size_t one_past_end)
{
    size_t size = one_past_end - start;
    //size_t insert_sort_size = 32 / sizeof(T);
    //if (insert_sort_size < 16)
    //    insert_sort_size = 16;
    size_t insert_sort_size = get_default_insertion_sort_size<T>();
    
    // pick an insertion sort size that guarantees that we won't need to swap the buffers an extra time at the end
    size_t temp = insert_sort_size;
    size_t tiers = 0;
    while (temp < size)
    {
        temp <<= 1;
        tiers += 1;
    }
    if (tiers & 1)
        insert_sort_size >>= 1;
    
    // divide array up into small chunks; use insertion sort on them
    for (size_t i = start; i < one_past_end; i += insert_sort_size)
    {
        size_t cap = i + insert_sort_size;
        if (cap > one_past_end)
            cap = one_past_end;
        insertion_sort_impl(f, data, i, cap);
    }
    for (size_t width = insert_sort_size; width < size; width <<= 1)
    {
        size_t i;
        for (i = start; i + width < one_past_end; i += width << 1)
        {
            size_t cap = i + (width << 1);
            if (cap > one_past_end)
                cap = one_past_end;
            
            merge_parts<false>(f, data, temp_buffer, i, cap, i + width, 1);
        }
        std::swap(data, temp_buffer);
        // if we didn't run the last iteration, perform its swap
        // this extra if causes gcc to emit slightly faster code for some reason
        if (i < one_past_end)
        {
            if constexpr (std::is_trivially_copyable<T>::value)
                memcpy(data + i, temp_buffer + i, sizeof(T) * (one_past_end - i));
            else
            {
                while (i < one_past_end)
                {
                    ::new((void*)(data + i)) T(std::move(temp_buffer[i]));
                    temp_buffer[i++].~T();
                }
            }
        }
    }
}

template<typename T, typename Comparator>
void merge_sort_bottomup_impl(Comparator f, T * data, T * temp_buffer, size_t start, size_t one_past_end)
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
        insertion_sort_impl(f, data, i, cap);
        
        // merge partial results into leftwards block as possible + while we go
        // this is more cache-coherent than the textbook "one layer at a time" bottom-up merge algorithm
        size_t blocks_n = blocks;
        size_t width = block_size;
        size_t i2 = i;
        T * buf_a = data;
        T * buf_b = temp_buffer;
        while (blocks_n & 1)
        {
            blocks_n >>= 1;
            i2 -= width;
            merge_parts<false>(f, buf_a, buf_b, i2, cap, i2 + width, 1);
            width <<= 1;
            std::swap(buf_a, buf_b);
        }
        blocks += 1;
    }
    // finish merging in the rightmost edge if our block count wasn't a power of 2
    if (blocks != (1U << tiers))
    {
        i -= block_size;
        T * buf_a = data;
        T * buf_b = temp_buffer;
        blocks -= 1;
        
        // skip already-done merge work
        while (blocks & 1)
        {
            blocks >>= 1;
            i -= block_size;
            block_size <<= 1;
            std::swap(buf_a, buf_b);
        }
        T * prev_write_buf = buf_a;
        
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
                merge_parts<false>(f, buf_a, buf_b, i, one_past_end, i + block_size, 1);
                prev_write_buf = buf_b;
            }
            block_size <<= 1;
            blocks >>= 1;
            std::swap(buf_a, buf_b);
        }
    }
}

template<typename T, typename Comparator>
void merge_sort_bottomup_inplace_impl(Comparator f, T * data, size_t start, size_t one_past_end)
{
    size_t size = one_past_end - start;
    size_t block_size = get_default_insertion_sort_size<T>();
    
    size_t temp = block_size;
    size_t tiers = 0;
    while ((temp << tiers) < size)
        tiers += 1;
    
    // divide array up into small chunks; use insertion sort on them
    size_t blocks = 0;
    size_t i;
    for (i = start; i < one_past_end; i += block_size)
    {
        size_t cap = i + block_size;
        if (cap > one_past_end)
            cap = one_past_end;
        insertion_sort_impl(f, data, i, cap);
        
        // merge partial results into leftwards block as possible + while we go
        // this is more cache-coherent than the textbook "one layer at a time" bottom-up merge algorithm
        size_t blocks_n = blocks;
        size_t width = block_size;
        size_t i2 = i;
        while (blocks_n & 1)
        {
            blocks_n >>= 1;
            i2 -= width;
            merge_parts_inplace(f, data, i2, cap, i2 + width);
            width <<= 1;
        }
        blocks += 1;
    }
    // finish merging in the rightmost edge if our array size wasn't a power-of-2 multiple of block_size
    if (blocks != (1U << tiers))
    {
        i -= block_size;
        blocks -= 1;
        
        // skip already-done merge work
        while (blocks & 1)
        {
            blocks >>= 1;
            i -= block_size;
            block_size <<= 1;
        }
        
        // finish up rest of merge work
        while (blocks > 0)
        {
            if (blocks & 1)
            {
                i -= block_size;
                merge_parts_inplace(f, data, i, one_past_end, i + block_size);
            }
            block_size <<= 1;
            blocks >>= 1;
        }
    }
}

// ####
// quick sort
// ####
#include <assert.h>
template<typename T, typename Comparator>
void quick_sort_impl(Comparator f, T * data, size_t start, size_t one_past_end)
{
    size_t insert_sort_size = get_default_insertion_sort_size<T>();
    
    // if small, use insertion sort (faster on small arrays for cache reasons)
    if (one_past_end - start <= insert_sort_size)
    {
        if (one_past_end - start >= 2)
            insertion_sort_impl(f, data, start, one_past_end);
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
    if (pivot == one_past_end - 1)
        pivot -= 1;
    
    // now perform partitioning
    
    // move pivot value to stack for better cache locality in loop
    std::swap(data[pivot], data[one_past_end - 1]);
    T pivot_val(std::move(data[one_past_end - 1]));
    
    // partition array into two parts separated by the pivot value
    size_t i = start;
    size_t j = one_past_end - 2;
    while (i < j)
    {
        while (i < one_past_end && f(data[i], pivot_val)) i += 1;
        while (j > start && f(pivot_val, data[j])) j -= 1;
        
        if (i < j)
            std::swap(data[i++], data[j--]);
    }
    // i is now the partition center index
    
    data[one_past_end - 1] = std::move(pivot_val);
    
    // if i == j, then the partition center value might be an arbitrary value higher or lower than the pivot.
    // lower is wrong. if lower, then to fix it, move partition center right by 1
    // all values to the right of i are greater than or equal to the pivot value, so this works
    if (i == j && i < one_past_end && f(data[i], data[one_past_end - 1]))
        i += 1;
    
    std::swap(data[i], data[one_past_end - 1]);
    
    quick_sort_impl(f, data, start, i);
    if (i + 1 < one_past_end)
        quick_sort_impl(f, data, i + 1, one_past_end);
}

#endif // _INCLUDE_BXX_SORTING
