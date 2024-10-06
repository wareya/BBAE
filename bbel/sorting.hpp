#ifndef _INCLUDE_BXX_SORTING
#define _INCLUDE_BXX_SORTING

#include <cstddef> // size_t
#include <utility> // std::move, std::swap

// ordering:
// insertion sort
// helpers
// merge sort (in-place)
// merge sort (temp buffer)
// quicksort

// ####
// insertion sort
// ####

template<typename T, typename Comparator>
void insertion_sort_impl(Comparator f, T * data, size_t start, size_t one_past_end)
{
    for(size_t i = start + 1; i < one_past_end; i += 1)
    {
        T item(std::move(data[i]));
        size_t j;
        for(j = i; j > start && f(item, data[j - 1]); j -= 1)
            data[j] = std::move(data[j - 1]);
        data[j] = std::move(item);
    }
}

// ####
// helpers
// ####

template<typename T, typename Comparator>
void shrink_sort_bounds(Comparator f, T * data, size_t & start, size_t & one_past_end)
{
    // despite the nested loops, this is an O(n) algorithm
    size_t start_p = start;
    while (one_past_end - start_p > 2 && !f(data[start_p + 1], data[start_p]))
        start_p += 1;
    for (size_t i = start_p; i < one_past_end && start_p > start; i += 1)
    {
        while (start_p > start && f(data[i], data[start_p]))
            start_p -= 1;
    }
    start = start_p;
    
    size_t end_p = one_past_end - 1;
    while (end_p - start > 2 && !f(data[end_p], data[end_p - 1]))
        end_p -= 1;
    for (size_t i = end_p - 1; i > start && end_p < one_past_end - 1; i -= 1)
    {
        while (end_p < one_past_end - 1 && f(data[end_p], data[i]))
            end_p += 1;
    }
    one_past_end = end_p + 1;
}

template<typename Comparator>
void bsearch(size_t & a, size_t & b, int round_up, int b_offs, Comparator f)
{
    while (a != b)
    {
        size_t avg = (a + b + round_up) / 2;
        if (f(avg)) a = avg;
        else        b = avg + b_offs;
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
// merge sort (in-place)
// ####

template<typename T, typename Comparator>
void merge_parts_inplace(Comparator f, T * data, size_t start, size_t one_past_end, size_t midpoint)
{
    // intution: by finding an inner window that we can safely rotate, we can divide the merging operation up
    //   into two smaller merging operations, the combination of which will take O(n) time.
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
    
    if ((one_past_end - start) * sizeof(T) < 128)
        return insertion_sort_impl(f, data, start, one_past_end);
    
    size_t size = 0;
    size_t size_hi = one_past_end - midpoint;
    if (midpoint - start < size_hi)
        size_hi = midpoint - start;
    
    // check if far ends ends are window size
    bsearch(size, size_hi, 1, -1, [&](auto avg)
        { return f(data[midpoint + avg - 1], data[midpoint - avg]); });
    
    if (size == 0)
        return;
    
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
        size_t left_lo = start;
        bsearch(left, left_lo, 0, 1, [&](auto avg)
            { return f(data[right - 1], data[avg]); });
    }
    if (right < one_past_end && left == start)
    {
        size_t right_hi = one_past_end;
        bsearch(right, right_hi, 1, -1, [&](auto avg)
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
    if (one_past_end - start <= 1)
        return;
    if ((one_past_end - start) * sizeof(T) <= 256)
        return insertion_sort_impl(f, data, start, one_past_end);
    
    size_t midpoint = start + (one_past_end - start) / 2;
    merge_sort_inplace_impl(f, data, start, midpoint);
    merge_sort_inplace_impl(f, data, midpoint, one_past_end);
    merge_parts_inplace(f, data, start, one_past_end, midpoint);
}

// ####
// merge sort (temp buffer)
// ####

template<typename T, typename Comparator>
void merge_parts(Comparator f, T * data, T * temp_buffer, size_t start, size_t one_past_end, size_t midpoint, char color)
{
    T * read_buf  = color ? data : temp_buffer;
    T * write_buf = color ? temp_buffer : data;
    
    if (midpoint - start < 4 || one_past_end - midpoint < 4 || (one_past_end - start) * sizeof(T) < 128)
    {
        insertion_sort_impl(f, data, start, one_past_end);
        
        size_t i = start;
        while (color && i < one_past_end)
        {
            ::new((void*)(write_buf + i)) T(std::move(read_buf[i]));
            read_buf[i++].~T();
        }
        return;
    }
    
    while (start < midpoint && !f(read_buf[midpoint], read_buf[start]))
    {
        ::new((void*)(write_buf + start)) T(std::move(read_buf[start]));
        read_buf[start++].~T();
    }
    while (one_past_end > midpoint && !f(read_buf[one_past_end - 1], read_buf[midpoint - 1]))
    {
        ::new((void*)(write_buf + (one_past_end - 1))) T(std::move(read_buf[one_past_end - 1]));
        read_buf[--one_past_end].~T();
    }
    
    size_t i = start;
    size_t j = midpoint;
    size_t k = start;
    while (k < one_past_end)
    {
        size_t index;
        if (j < one_past_end && (i == midpoint || f(read_buf[j], read_buf[i])))
            index = j++;
        else
            index = i++;
        
        ::new((void*)(write_buf + (k++))) T(std::move(read_buf[index]));
        read_buf[index].~T();
    }
}

template<typename T, typename Comparator>
void merge_sort_impl(Comparator f, T * data, T * temp_buffer, size_t start, size_t one_past_end, char color)
{
    if ((one_past_end - start) * sizeof(T) <= 128 || one_past_end - start < 8)
    {
        if (one_past_end - start >= 2)
            insertion_sort_impl(f, data, start, one_past_end);
        
        size_t i = start;
        while (color && i < one_past_end)
        {
            ::new((void*)(temp_buffer + i)) T(std::move(data[i]));
            data[i++].~T();
        }
        return;
    }
    
    size_t midpoint = start + (one_past_end - start) / 2;
    merge_sort_impl(f, data, temp_buffer, start, midpoint, !color);
    merge_sort_impl(f, data, temp_buffer, midpoint, one_past_end, !color);
    merge_parts(f, data, temp_buffer, start, one_past_end, midpoint, color);
}

// ####
// quick sort
// ####

template<typename T, typename Comparator>
void quick_sort_impl(Comparator f, T * data, size_t start, size_t one_past_end)
{
    if ((one_past_end - start) * sizeof(T) <= 256 || one_past_end - start < 8)
    {
        if (one_past_end - start >= 2)
            insertion_sort_impl(f, data, start, one_past_end);
        return;
    }
    
    // swap opposing unsorted elements to reduce the negative effect of poor pivot selection
    size_t length = one_past_end - start;
    for (size_t i = start; i < start + length / 2; i++)
    {
        size_t i2 = start + length - (i - start) - 1;
        if (f(data[i2], data[i]))
            std::swap(data[i], data[i2]);
    }
    
    // pick middle position as pivot for the sake of simplicity
    size_t pivot = data[(start + one_past_end) / 2];
    
    size_t i = start;
    size_t j = one_past_end - 1;
    while (i < j)
    {
        while (f(data[i], pivot)) i += 1;
        while (f(pivot, data[j])) j -= 1;
        
        if (i < j)
            std::swap(data[i++], data[j--]);
    }
    
    quick_sort_impl(f, data, start, i);
    quick_sort_impl(f, data, i, one_past_end);
}

#endif // _INCLUDE_BXX_SORTING
