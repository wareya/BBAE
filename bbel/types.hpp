#ifndef _INCLUDE_BXX_TYPES
#define _INCLUDE_BXX_TYPES

#include <cmath>
#include <cstdio> // size_t, malloc/free
#include <cstdlib> // size_t, malloc/free
#include <cstring> // memcpy, memmove
#include <cstddef> // nullptr_t

#include <utility> // std::move, std::forward
#include <initializer_list> // initializer list constructors
#include <new> // placement new

// ####
// helpers
// ####

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

#include "sorting.hpp"

template<typename T>
class Shared {
    class SharedInner {
    public:
        friend Shared;
        T item;
        size_t refcount = 0;
    };
    SharedInner * inner = nullptr;
    
public:
    constexpr explicit operator bool() const noexcept { return inner && inner->refcount > 0; }
    constexpr const T & operator*() const { if (!inner) throw; return inner->item; }
    constexpr T & operator*() { if (!inner) throw; return inner->item; }
    constexpr const T * operator->() const noexcept { return inner ? &inner->item : nullptr; }
    constexpr T * operator->() noexcept { return inner ? &inner->item : nullptr; }
    
    constexpr bool operator==(const Shared & other) const { return inner == other.inner; }
    constexpr bool operator==(std::nullptr_t) const { return !*this; }
    constexpr bool operator!=(std::nullptr_t) const { return !!*this; }
    
    Shared & operator=(Shared && other)
    {
        if (inner)
        {
            inner->refcount -= 1;
            if (inner->refcount == 0)
                delete inner;
        }
        
        inner = other.inner;
        other.inner = nullptr;
        
        return *this;
    };
    Shared & operator=(const Shared & other)
    {
        if (other.inner == inner)
            return *this;
        
        if (inner)
        {
            inner->refcount -= 1;
            if (inner->refcount == 0)
                delete inner;
        }
        
        inner = other.inner;
        if (inner)
            inner->refcount += 1;
        
        return *this;
    };
    
    constexpr Shared() noexcept { }
    constexpr Shared(std::nullptr_t) noexcept { }
    constexpr Shared(T && from) noexcept
    {
        inner = new SharedInner({std::move(from), 1});
    }
    constexpr Shared(Shared && other) noexcept
    {
        inner = other.inner;
        other.inner = nullptr;
    }
    constexpr Shared(const Shared & other) noexcept
    {
        inner = other.inner;
        if (inner)
            inner->refcount += 1;
    }
    ~Shared()
    {
        if (!inner) return;
        inner->refcount -= 1;
        if (inner->refcount == 0)
            delete inner;
    }
    
    T * get() const
    {
        if (inner) return &(inner->item);
        return 0;
    }
};

template<typename T, class... Args>
constexpr static Shared<T> MakeShared(Args &&... args) noexcept
{
    T obj = T{std::forward<Args>(args)...};
    Shared<T> ret(std::move(obj));
    return ret;
}

template<typename T>
class CopyableUnique {
    T * data = nullptr;
public:
    constexpr explicit operator bool() const noexcept { return data; }
    constexpr const T & operator*() const { if (!data) throw; return *data; }
    constexpr T & operator*() { if (!data) throw; return *data; }
    constexpr const T * operator->() const noexcept { return data; }
    constexpr T * operator->() noexcept { return data; }
    constexpr bool operator==(const CopyableUnique & other) const { return data == other.data; }
    constexpr bool operator==(std::nullptr_t) const { return !!data; }
    constexpr bool operator!=(std::nullptr_t) const { return !data; }
    CopyableUnique & operator=(CopyableUnique && other)
    {
        if (other.data == data)
            return *this;
        
        if (data)
            delete data;
        
        data = other.data;
        other.data = nullptr;
        
        return *this;
    };
    CopyableUnique & operator=(const CopyableUnique & other)
    {
        if (*this == other)
            return *this;
        
        if (data)
            delete data;
        
        if (other.data)
            data = new T(*other.data);
        else
            data = nullptr;
        
        return *this;
    };
    constexpr CopyableUnique() noexcept { }
    constexpr CopyableUnique(std::nullptr_t) noexcept { }
    constexpr CopyableUnique(T && from) noexcept
    {
        data = new T(std::move(from));
    }
    constexpr CopyableUnique(CopyableUnique && other) noexcept
    {
        data = other.data;
        other.data = nullptr;
    }
    constexpr CopyableUnique(const CopyableUnique & other) noexcept
    {
        if (other.data)
            data = new T(*other.data);
    }
    ~CopyableUnique()
    {
        if (data)
            delete data;
    }
    
    T * get() const
    {
        return data;
    }
};

template<typename T, class... Args>
constexpr static CopyableUnique<T> MakeUnique(Args &&... args) noexcept
{
    T obj = T{std::forward<Args>(args)...};
    CopyableUnique<T> ret(std::move(obj));
    return ret;
}

template<typename T>
class alignas(T) Option {
    char data[sizeof(T)];
    bool isok = false;
    
public:
    
    constexpr explicit operator bool() const noexcept { return isok; }
    constexpr const T & operator*() const noexcept { return *(T*)data; }
    constexpr T & operator*() noexcept { return *(T*)data; }
    constexpr const T * operator->() const noexcept { return (T*)data; }
    constexpr T * operator->() noexcept { return (T*)data; }
    Option & operator=(Option &&) = default;
    
    constexpr Option() noexcept { }
    
    template <typename U>
    constexpr Option(U && item)
    {
        ::new((void*)(T*)data) T(std::forward<U>(item));
        isok = true;
    }
    
    constexpr Option(Option && other)
    {
        if (other.isok)
            ::new((void*)(T*)data) T(std::move(*(T*)other.data));
        isok = other.isok;
    }
    
    constexpr Option(const Option & other)
    {
        if (other.isok)
            ::new((void*)(T*)data) T(*(T*)other.data);
        isok = other.isok;
    }
    
    ~Option()
    {
        if (isok)
            ((T*)data)->~T();
    }
};

template<typename T0, typename T1>
struct Pair {
    T0 _0;
    T1 _1;
    Pair(const T0 & a, const T1 & b) : _0(a), _1(b) { }
    Pair(T0 && a, T1 && b) : _0(std::move(a)), _1(std::move(b)) { }
    Pair(const T0 & a, T1 && b) : _0(a), _1(std::move(b)) { }
    Pair(T0 && a, const T1 & b) : _0(std::move(a)), _1(b) { }
    Pair & operator==(const Pair & other) const
    {
        return _0 == other._0 && _1 == other._1;
    }
};

template<typename T>
class Vec {
private:
    size_t mlength = 0;
    size_t mcapacity = 0;
    char * mbuffer = nullptr;
    char * mbuffer_raw = nullptr;
public:
    size_t size() const noexcept { return mlength; }
    size_t capacity() const noexcept { return mcapacity; }
    T * data() noexcept { return (T*)mbuffer; }
    const T * data() const noexcept { return (T*)mbuffer; }
    T * begin() noexcept { return (T*)mbuffer; }
    T * end() noexcept { return ((T*)mbuffer) + mlength; }
    const T * begin() const noexcept { return (T*)mbuffer; }
    const T * end() const noexcept { return ((T*)mbuffer) + mlength; }
    
    T & front() { if (mlength == 0) throw; return ((T*)mbuffer)[0]; }
    T & back() { if (mlength == 0) throw; return ((T*)mbuffer)[mlength - 1]; }
    const T & front() const { if (mlength == 0) throw; return ((T*)mbuffer)[0]; }
    const T & back() const { if (mlength == 0) throw; return ((T*)mbuffer)[mlength - 1]; }
    
    const T & operator[](size_t pos) const noexcept { return ((T*)mbuffer)[pos]; }
    T & operator[](size_t pos) noexcept { return ((T*)mbuffer)[pos]; }
    
    constexpr bool operator==(const Vec<T> & other) const
    {
        if (other.mlength != mlength)
            return false;
        for (size_t i = 0; i < mlength; i++)
        {
            if (!((*this)[i] == other[i]))
                return false;
        }
        return true;
    }
    // Constructors
    
    // Blank
    
    constexpr Vec() noexcept { }
    
    // With capacity hint
    
    constexpr explicit Vec(size_t count)
    {
        apply_cap_hint(count);
    }
    
    // Filled with default
    
    constexpr Vec(size_t count, T default_val)
    {
        apply_cap_hint(count);
        mlength = count;
        
        for (size_t i = 0; i < mlength; i++)
            ::new((void*)(((T*)mbuffer) + i)) T(default_val);
    }
    
    // Copy
    
    /// If the copy constructor of T throws an exception during operation, the state of the container is undefined.
    //template<typename = typename std::enable_if<std::is_copy_constructible<T>::value>::type>
    constexpr Vec(const Vec & other)
    {
        apply_cap_hint(other.capacity());
        copy_into(*this, other);
    }
    constexpr Vec(Vec && other) noexcept
    {
        mlength = other.mlength;
        mcapacity = other.mcapacity;
        mbuffer = other.mbuffer;
        mbuffer_raw = other.mbuffer_raw;
        other.mlength = 0;
        other.mcapacity = 0;
        other.mbuffer = nullptr;
        other.mbuffer_raw = nullptr;
    }
    template<class Iterator>
    constexpr Vec(Iterator first, Iterator past_end)
    {
        while (first != past_end)
        {
            push_back(*first);
            first++;
        }
    }
    constexpr Vec(std::initializer_list<T> initializer)
    {
        apply_cap_hint(initializer.size());
        auto first = initializer.begin();
        auto last = initializer.end();
        while (first != last)
        {
            push_back(*first);
            first++;
        }
    }
    
    // Destructor
    
    ~Vec()
    {
        for (size_t i = 0; i < mlength; i++)
            ((T*)mbuffer)[i].~T();
        if (mbuffer_raw)
            free(mbuffer_raw);
    }
    
    /// If the copy constructor of T throws an exception during operation, the state of the container is undefined.
    //template<typename = typename std::enable_if<std::is_copy_constructible<T>::value>::type>
    constexpr Vec & operator=(const Vec & other)
    {
        if (this == &other)
            return *this;
        
        if (mbuffer_raw)
        {
            for (size_t i = 0; i < mlength; i++)
                ((T*)mbuffer)[i].~T();
            free(mbuffer_raw);
            mlength = 0;
            mbuffer = 0;
            mbuffer_raw = 0;
        }
        
        reserve(other.size());
        copy_into(*this, other);
        
        return *this;
    }
    constexpr Vec & operator=(Vec && other) noexcept
    {
        if (this == &other)
            return *this;
        
        mlength = other.mlength;
        mcapacity = other.mcapacity;
        mbuffer = other.mbuffer;
        mbuffer_raw = other.mbuffer_raw;
        other.mlength = 0;
        other.mcapacity = 0;
        other.mbuffer = nullptr;
        other.mbuffer_raw = nullptr;
        
        return *this;
    }
    
    // API
    
    /// If T's move constructor throws during operation, the container is invalid (but destructible).
    constexpr void reserve(size_t new_cap)
    {
        if (new_cap > mlength)
            do_realloc(new_cap);
    }
    
    void swap_contents(Vec & other)
    {
        size_t temp_mlength = other.mlength;
        size_t temp_mcapacity = other.mcapacity;
        char * temp_mbuffer = other.mbuffer;
        char * temp_mbuffer_raw = other.mbuffer_raw;
        
        other.mlength = mlength;
        other.mcapacity = mcapacity;
        other.mbuffer = mbuffer;
        other.mbuffer_raw = mbuffer_raw;
        
        mlength = temp_mlength;
        mcapacity = temp_mcapacity;
        mbuffer = temp_mbuffer;
        mbuffer_raw = temp_mbuffer_raw;
    }
    
private:
    void prepare_insert(size_t i)
    {
        if (mlength >= mcapacity)
        {
            mcapacity = mcapacity == 0 ? 1 : (mcapacity << 1);
            do_realloc(mcapacity);
        }
        
        mlength += 1;
        
        for (size_t j = mlength - 1; j > i; j--)
        {
            ::new((void*)(((T*)mbuffer) + j)) T(std::move(((T*)mbuffer)[j - 1]));
            ((T*)mbuffer)[j - 1].~T();
        }
    }
public:
    /// If T's move or copy constructors throw during operation, one of the following happens:
    /// 1) If i is size() - 1 (before calling), the container is unaffected, except its capacity may be increased.
    /// 2) Otherwise, the container's state is undefined.
    /// i must be less than or equal to size().
    template <typename U>
    void insert_at(size_t i, U && item)
    {
        prepare_insert(i);
        try
        {
            ::new((void*)(((T*)mbuffer) + i)) T(std::forward<U>(item));
        }
        catch (...)
        {
            mlength -= 1;
            throw;
        }
    }
    void push_back(T && item)
    {
        insert_at(size(), item);
    }
    void push_back(const T & item)
    {
        insert_at(size(), item);
    }
    /// If T's move constructor throws during operation, one of the following happens:
    /// 1) If i is size() - 1 (before calling), the container is unaffected.
    /// 2) Otherwise, the container's state is undefined.
    /// i must be less than size().
    constexpr T erase_at(size_t i)
    {
        if (i >= mlength)
            throw;
        
        T ret(std::move(((T*)mbuffer)[i]));
        
        for (size_t j = i; j + 1 < mlength; j++)
        {
            ((T*)mbuffer)[j].~T();
            ::new((void*)(((T*)mbuffer) + j)) T(std::move(((T*)mbuffer)[j + 1]));
        }
        
        ((T*)mbuffer)[mlength - 1].~T();
        mlength -= 1;
        maybe_shrink();
        
        return ret;
    }
    /// size() must be at least 1.
    constexpr T pop_back()
    {
        return erase_at(size() - 1);
    }
    /// Pointer must be within data().
    constexpr void erase(const T * which)
    {
        size_t i = which - (T*)mbuffer;
        erase_at(i);
    }
    /// Pointer must be within data().
    constexpr void erase(T * which)
    {
        size_t i = which - (T*)mbuffer;
        erase_at(i);
    }
    
    Vec split_at(ptrdiff_t i)
    {
        if (i < 0)
            i += size();
        if ((size_t)i >= size())
            return Vec();
        
        Vec ret;
        ret.reserve(size() - i);
        
        for (size_t j = i; j < mlength; j++)
        {
            ::new((void*)(ret.data() + (j - i))) T(std::move(data()[j]));
            data()[j].~T();
        }
        
        ret.mlength = mlength - i;
        mlength -= mlength - i;
        apply_cap_hint(mlength);
        
        return ret;
    }
    
    /// Performs merge sort with auxiliary memory.
    /// Merge sort is a sorting algorithm with:
    /// - Downside: temporarily allocates size() extra items worth of memory
    /// - All other properties are (nearly) ideal
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    /// This is an adaptive merge sort that sorts sorted and mostly-sorted arrays faster than simple merge sorts.
    template<typename Comparator>
    void merge_sort(Comparator f)
    {
        if (size() * sizeof(T) <= 128 || size() < 8)
            return insertion_sort(f);
        Vec<T> temp;
        temp.reserve(size());
        size_t start = 0;
        size_t one_past_end = size();
        shrink_sort_bounds<T>(f, data(), start, one_past_end);
        merge_sort_bottomup_impl<T>(f, data(), temp.data(), start, one_past_end);
    }
    /// Performs insertion sort.
    /// Insertion sort is a sorting algorithm with:
    /// - Downside: spends up to n^2 time, where n increases linearly with Vec size
    /// - All other properties are (nearly) ideal
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    /// Insertion sort is inherently adaptive and runs in O(n) time if the list is already sorted.
    template<typename Comparator>
    void insertion_sort(Comparator f)
    {
        insertion_sort_impl<T>(f, data(), 0, size());
    }
    /// Performs quicksort.
    /// quicksort is a sorting algorithm with:
    /// - Downside: "unstable" (does not retain order of equally-ordered elements).
    /// - Downside: Can take more than O(n logn) time for specially-crafted malicious inputs, but is usually ideally fast.
    /// - All other properties are (nearly) ideal
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    /// This is an adaptive variant that skips the beginning and end of the list if they're already sorted.
    template<typename Comparator>
    void quick_sort(Comparator f)
    {
        size_t start = 0;
        size_t one_past_end = size();
        shrink_sort_bounds<T>(f, data(), start, one_past_end);
        quick_sort_impl<T>(f, data(), start, one_past_end);
    }
    /// Sorts the Vec.
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    template<typename Comparator>
    void sort(Comparator f)
    {
        this->merge_sort(f);
    }
    
private:
    
    void apply_cap_hint(size_t count)
    {
        mcapacity = count > 0 ? 1 : 0;
        while (mcapacity < count)
            mcapacity = (mcapacity << 1) >= count ? (mcapacity << 1) : count;
        do_realloc(mcapacity);
    }
    
    static void copy_into(Vec & a, const Vec & b)
    {
        a.mlength = b.size();
        for (size_t i = 0; i < a.mlength; i++)
            ::new((void*)(((T*)a.mbuffer) + i)) T(b[i]);
    }
    
    void maybe_shrink()
    {
        if (mlength < (mcapacity >> 2))
            do_realloc(mcapacity >> 1);
    }
    void do_realloc(size_t new_capacity)
    {
        mcapacity = new_capacity;
        if (mlength > mcapacity) throw;
        char * new_buf_raw = mcapacity ? (char *)malloc(mcapacity * sizeof(T) + alignof(T)) : nullptr;
        if (mcapacity && !new_buf_raw) throw;
        char * new_buf = new_buf_raw;
        while (size_t(new_buf) % alignof(T))
            new_buf += 1;
        for (size_t i = 0; i < mlength; i++)
            ::new((void*)(((T*)new_buf) + i)) T(std::move(((T*)mbuffer)[i]));
        for (size_t i = 0; i < mlength; i++)
            ((T*)mbuffer)[i].~T();
        if (mbuffer_raw) free(mbuffer_raw);
        mbuffer_raw = new_buf_raw;
        mbuffer = new_buf;
    }
};


//template<typename T, int min_size = 16, int max_size = 64>
//template<typename T, int min_size = 32, int max_size = 128>
//template<typename T, int min_size = 64, int max_size = 256>
template<typename T, int min_size = 128, int max_size = 512>
//template<typename T, int min_size = 256, int max_size = 1024>
//template<typename T, int min_size = 512, int max_size = 2048>
class Rope {
private:
    // minimum chunk size before merging children together
    static const int min_chunk_size = min_size;
    // maximum chunk size before splitting
    static const int split_chunk_size = max_size;
    class RopeNode {
    public:
        size_t mlength = 0;
        size_t mheight = 0;
        size_t mleaves = 1;
        Vec<T> items = {};
        Shared<RopeNode> left = 0;
        Shared<RopeNode> right = 0;
        
        template<typename D>
        static RopeNode from_copy(const D data, size_t start, size_t count)
        {
            RopeNode ret;
            if (count <= max_size)
            {
                for (size_t i = 0; i < count; i++)
                    ret.items.push_back(data[start + i]);
                
                ret.mlength = count;
                return ret;
            }
            ret.left = from_copy(data, start, count / 2);
            ret.right = from_copy(data, start + count / 2, count - count / 2);
            ret.fix_metadata();
            return ret;
        }
        
        template<typename D>
        static RopeNode from_move(const D data, size_t start, size_t count)
        {
            RopeNode ret;
            if (count <= max_size)
            {
                for (size_t i = 0; i < count; i++)
                    ret.items.push_back(std::move(data[start + i]));
                
                ret.mlength = count;
                return ret;
            }
            ret.left = from_move(data, start, count / 2);
            ret.right = from_move(data, start + count / 2, count - count / 2);
            ret.fix_metadata();
            return ret;
        }
        
        size_t size() const
        {
            return mlength;
        }
        const T & get_at(size_t i) const
        {
            if (i < items.size())
                return items[i];
            else if (i < left->size())
                return left->get_at(i);
            else
                return right->get_at(i - left->size());
        }
        T & get_at(size_t i)
        {
            if (i < items.size())
                return items[i];
            else if (i < left->size())
                return left->get_at(i);
            else
                return right->get_at(i - left->size());
        }
        const T & operator[](size_t pos) const { return get_at(pos); }
        T & operator[](size_t pos) { return get_at(pos); }
        
        size_t calc_size() const
        {
            if (left || right)
            {
                if (!(left && right)) throw;
                
                return left->calc_size() + right->calc_size();
            }
            return items.size();
        }
        
        T erase_at(size_t i, Shared<RopeNode> * where, size_t * where_start)
        {
            if (!(i < mlength)) throw;
            
            T item = [&]()
            {
                if (left && i < left->size())
                {
                    T item(left->erase_at(i, where, where_start));
                    
                    if (where && !left->left)
                        *where = left;
                    
                    mheight = right->mheight + 1;
                    if (mheight < left->mheight + 1)
                        mheight = left->mheight + 1;
                    mleaves = left->mleaves + right->mleaves;
                    return item;
                }
                else if (right)// && i - left->size() < right->size())
                {
                    T item(right->erase_at(i - left->size(), where, where_start));
                    
                    if (where && !right->left)
                        *where = right;
                    if (where_start)
                        *where_start += left->size();
                    
                    mheight = left->mheight + 1;
                    if (mheight < right->mheight + 1)
                        mheight = right->mheight + 1;
                    mleaves = left->mleaves + right->mleaves;
                    return item;
                }
                else
                {
                    if(!(i < items.size())) throw;
                    T item(items.erase_at(i));
                    return item;
                }
            }();
            
            mlength -= 1;
            
            if ((left || right) && mlength < min_chunk_size)
            {
                for (size_t i = 0; i < mlength; i++)
                    items.push_back(std::move(get_at(i)));
                
                left = 0;
                right = 0;
                mheight = 0;
                mleaves = 1;
            }
            
            return item;
        }
        
        void insert_at(size_t i, T item, Shared<RopeNode> * where, size_t * where_start)
        {
            if (left && i <= left->size())
            {
                left->insert_at(i, item, where, where_start);
                
                if (where && !left->left)
                    *where = left;
                
                if (mheight < left->mheight + 1)
                    mheight = left->mheight + 1;
                mleaves = left->mleaves + right->mleaves;
                mlength += 1;
            }
            else if (right)// && i - left->size() <= right->size())
            {
                right->insert_at(i - left->size(), item, where, where_start);
                
                if (where && !right->left)
                    *where = right;
                if (where_start)
                    *where_start += left->size();
                
                if (mheight < right->mheight + 1)
                    mheight = right->mheight + 1;
                mleaves = left->mleaves + right->mleaves;
                mlength += 1;
            }
            else
            {
                if (!(i <= items.size())) throw;
                items.insert_at(i, item);
                
                if (items.size() > split_chunk_size)
                {
                    left = MakeShared<RopeNode>();
                    right = MakeShared<RopeNode>();
                    
                    left->items = std::move(items);
                    right->items = left->items.split_at(left->items.size() / 2);
                    
                    left->mlength = left->items.size();
                    right->mlength = right->items.size();
                    
                    mheight = 1;
                    mleaves = 2;
                }
                
                mlength += 1;
            }
        }
        
        void update_length_into(size_t i)
        {
            if (left && i <= left->size())
                left->update_length_into(i);
            else if (right)
                right->update_length_into(i - left->size());
            else
                return;
            mlength = left->mlength + right->mlength;
        }
        
        void print_structure(size_t depth) const
        {
            for (size_t i = 0; i < depth; i++)
                printf("-");
            if (left)
            {
                printf("branch\n");
                left->print_structure(depth + 1);
                right->print_structure(depth + 1);
                return;
            }
            printf("leaf: `");
            for (size_t i = 0; i < items.size(); i++)
                printf("%c", items[i]);
            puts("`");
        }
        void fix_metadata()
        {
            if (left)
            {
                if (!right) throw;
                mheight = left->mheight + 1;
                if (mheight < right->mheight + 1)
                    mheight = right->mheight + 1;
                mlength = left->size() + right->size();
                mleaves = left->mleaves + right->mleaves;
            }
            else
            {
                mheight = 0;
            }
        }
        void rebalance_impl()
        {
            if (mheight == 0)
            {
                fix_metadata();
                return;
            }
            //size_t start_len = size();
            if (mheight <= 2)
            {
                //puts("null case!");
                fix_metadata();
                //assert(start_len == size());
                return;
            }
            
            //assert(size() == left->size() + right->size());
            //
            //if (left || right)
            //    assert(left && right);
            
            // main cases
            size_t height_a = 0;
            size_t height_b = 0;
            size_t height_c = 0;
            if (left->mheight >= right->mheight)
            {
                height_a = left->left ? left->left->mheight + 1 : 0;
                height_b = left->right ? left->right->mheight + 1 : 0;
                height_c = right->mheight;
            }
            else
            {
                height_a = left->mheight;
                height_b = right->left->mheight + 1;
                height_c = right->right->mheight + 1;
            }
            // case A: a and c same but b shorter. rebalance whichever side b came from.
            //if (height_a == height_c && height_a >= height_b)
            if (height_a == height_c && height_a > height_b)
            {
                //puts("case A!");
                if (left->mheight >= right->mheight)
                    left->rebalance_impl();
                else
                    right->rebalance_impl();
                fix_metadata();
                //assert(start_len == size());
                return;
            }
            // case B: A or C is highest, A and C different. need to rotate
            if (height_a > height_b || height_c > height_b)
            {
                //puts("case B!");
                // don't want to use left->mheight etc here because in edge cases it can cause antibalancing rotations
                // FIXME: should we handle the (a<b=c) and (a=b>c) cases in "case A"?
                //  (the difference is which side gets rebalanced)
                if (height_a >= height_c)
                {
                    Shared<RopeNode> a(std::move(left->left));
                    Shared<RopeNode> b(std::move(left->right));
                    Shared<RopeNode> c(std::move(right));
                    
                    right = std::move(left);
                    
                    left = std::move(a);
                    
                    right->left = std::move(b);
                    right->right = std::move(c);
                    
                    right->fix_metadata();
                    left->rebalance_impl();
                }
                else
                {
                    Shared<RopeNode> a(std::move(left));
                    Shared<RopeNode> b(std::move(right->left));
                    Shared<RopeNode> c(std::move(right->right));
                    
                    left = std::move(right);
                    
                    left->left = std::move(a);
                    left->right = std::move(b);
                    right = std::move(c);
                    
                    left->fix_metadata();
                    right->rebalance_impl();
                }
                
                fix_metadata();
                return;
            }
            
            // case C: B is highest or tied. need to split and prepend/append
            if (height_b >= height_a && height_b >= height_c)
            {
                //puts("case C!");
                auto l = MakeShared<RopeNode>();
                auto r = MakeShared<RopeNode>();
                
                if (left->mheight >= right->mheight)
                {
                    l->left = std::move(left->left);
                    l->right = std::move(left->right->left);
                    r->left = std::move(left->right->right);
                    r->right = std::move(right);
                }
                else
                {
                    l->left = std::move(left);
                    l->right = std::move(right->left->left);
                    r->left = std::move(right->left->right);
                    r->right = std::move(right->right);
                }
                
                left = std::move(l);
                right = std::move(r);
                
                left->fix_metadata();
                right->fix_metadata();
                
                if (left->mheight >= right->mheight)
                    left->rebalance_impl();
                else
                    right->rebalance_impl();
                
                fix_metadata();
                return;
            }
            
            // case D: FIXME
            printf("FIXME/TODO: %zu %zu %zu\n", height_a, height_b, height_c);
            throw;
        }
    };
    
    //__attribute__((noinline))
    void rebalance()
    {
        if (!root)
            return;
        
        //size_t s = (root->size() + (min_chunk_size - 1)) / min_chunk_size + 1;
        //size_t s = (root->size() + 7) / 8;
        
        size_t s = root->mleaves;
        s += 1;
        
        // log of base phi approximation
        size_t logn = 1;
        while (s >>= 1)
            logn += 1;
        logn = logn * 23 / 16;
        
        if (root->mheight > logn)
        {
            if constexpr(1)
            {
                root->rebalance_impl();
            }
            else
            {
                auto new_root = MakeShared<RopeNode>(std::move(RopeNode::from_move(*root, 0, root->mlength)));
                root = new_root;
            }
            num_rebalances += 1;
        }
    }
    
    Shared<RopeNode> root = 0;
    
    size_t cached_node_start = 0;
    size_t cached_node_len = 0;
    Shared<RopeNode> cached_node = 0;
    
    size_t insert_cached_node_start = 0;
    size_t insert_cached_node_len = 0;
    Shared<RopeNode> insert_cached_node = 0;
    
    size_t erase_cached_node_start = 0;
    size_t erase_cached_node_len = 0;
    Shared<RopeNode> erase_cached_node = 0;
    
    void kill_cache()
    {
        cached_node = 0;
        insert_cached_node = 0;
    }
    
    void fix_cache(size_t prev_leaves, size_t i, ptrdiff_t diff)
    {
        if (prev_leaves != root->mleaves)
            kill_cache();
        
        if (diff > 0)
        {
            if (cached_node && i < cached_node_start)
                cached_node_start += diff;
            if (insert_cached_node && i < insert_cached_node_start)
                insert_cached_node_start += diff;
            if (erase_cached_node && i < erase_cached_node_start)
                erase_cached_node_start += diff;
        }
        else
        {
            if (cached_node && i < cached_node_start)
            {
                if (i - diff >= cached_node_start)
                    cached_node = 0;
                else
                    cached_node_start -= diff;
            }
            if (insert_cached_node && i < insert_cached_node_start)
            {
                if (i - diff >= insert_cached_node_start)
                    insert_cached_node = 0;
                else
                    insert_cached_node_start -= diff;
            }
            if (erase_cached_node && i < erase_cached_node_start)
            {
                if (i - diff >= erase_cached_node_start)
                    erase_cached_node = 0;
                else
                    erase_cached_node_start -= diff;
            }
        }
    }
    
public:
    
    size_t num_rebalances = 0;
    
    Rope() = default;
    Rope(const Rope &) = default;
    Rope(Rope &&) = default;
    
    Rope & operator=(Rope && other) = default;
    Rope & operator=(const Rope & other)
    {
        root = MakeShared<RopeNode>(std::move(RopeNode::from_copy(*other.root, 0, other.size())));
        return *this;
    };
    
    T & operator[](size_t pos)
    {
        if (!root) throw;
        
        if (cached_node && (pos - cached_node_start) < cached_node_len)
            return (*cached_node)[pos - cached_node_start];
        if (insert_cached_node && (pos - insert_cached_node_start) < insert_cached_node_len)
            return (*insert_cached_node)[pos - insert_cached_node_start];
        if (erase_cached_node && (pos - erase_cached_node_start) < erase_cached_node_len)
            return (*erase_cached_node)[pos - erase_cached_node_start];
        
        cached_node_start = 0;
        cached_node = root;
        size_t i = pos;
        while (cached_node->left)
        {
            if (i < cached_node->left->size())
                cached_node = cached_node->left;
            else
            {
                i -= cached_node->left->size();
                cached_node_start += cached_node->left->size();
                cached_node = cached_node->right;
            }
        }
        cached_node_len = cached_node->size();
        
        return (*cached_node)[pos - cached_node_start];
    }
    
    size_t size() const
    {
        if (root)
            return root->mlength;
        return 0;
    }
    void insert_at(size_t i, T item)
    {
        if (!root)
            root = MakeShared<RopeNode>();
        
        Shared<RopeNode> where;
        size_t where_start = 0;
        
        if (cached_node && i <= cached_node_start + cached_node_len
            && i >= cached_node_start && cached_node->size() < split_chunk_size)
        {
            where = cached_node;
            where_start = cached_node_start;
        }
        else if (insert_cached_node && i <= insert_cached_node_start + insert_cached_node_len
            && i >= insert_cached_node_start && insert_cached_node->size() < split_chunk_size)
        {
            where = insert_cached_node;
            where_start = insert_cached_node_start;
        }
        else if (erase_cached_node && i <= erase_cached_node_start + erase_cached_node_len
            && i >= erase_cached_node_start && erase_cached_node->size() < split_chunk_size)
        {
            where = erase_cached_node;
            where_start = erase_cached_node_start;
        }
        
        size_t prev_leaves = root->mleaves;
        if (where)
        {
            where->insert_at(i - where_start, item, 0, 0);
            root->update_length_into(i);
            rebalance();
            fix_cache(prev_leaves, i, 1);
            return;
        }
        
        root->insert_at(i, item, &where, &where_start);
        
        if (where)
        {
            insert_cached_node = where;
            insert_cached_node_start = where_start;
            insert_cached_node_len = where->size();
        }
        
        fix_cache(prev_leaves, i, 1);
        
        rebalance();
    }
    void insert(size_t i, T item)
    {
        insert_at(i, item);
    }
    T erase_at(size_t i)
    {
        if (!root)
            throw;
        
        
        Shared<RopeNode> where;
        size_t where_start = 0;
        
        if (cached_node && i < cached_node_start + cached_node_len
            && i >= cached_node_start && cached_node->size() > min_chunk_size)
        {
            where = cached_node;
            where_start = cached_node_start;
        }
        else if (insert_cached_node && i < insert_cached_node_start + insert_cached_node_len
            && i >= insert_cached_node_start && insert_cached_node->size() > min_chunk_size)
        {
            where = insert_cached_node;
            where_start = insert_cached_node_start;
        }
        else if (erase_cached_node && i < erase_cached_node_start + erase_cached_node_len
            && i >= erase_cached_node_start && erase_cached_node->size() > min_chunk_size)
        {
            where = erase_cached_node;
            where_start = erase_cached_node_start;
        }
        
        size_t prev_leaves = root->mleaves;
        if (where)
        {
            auto ret = where->erase_at(i - where_start, 0, 0);
            root->update_length_into(i);
            rebalance();
            fix_cache(prev_leaves, i, -1);
            return ret;
        }
        
        T ret = root->erase_at(i, &where, &where_start);
        
        if (where)
        {
            erase_cached_node = where;
            erase_cached_node_start = where_start;
            erase_cached_node_len = where->size();
        }
        
        fix_cache(prev_leaves, i, -1);
        
        rebalance();
        return ret;
    }
    void erase(size_t i)
    {
        erase_at(i);
    }
    void erase(size_t i, size_t count)
    {
        for (size_t n = 0; n < count; n++)
            erase_at(i);
    }
    void print_structure() const
    {
        if (!root)
            return (void)puts("empty");
        root->print_structure(0);
    }
    /// Sorts the rope.
    /// Uses insertion sort, which, for ropes, is O(n logn) in the worst case and O(n) in the best case.
    /// Insertion sort is the ideal sorting algorithm for ropes.
    /// - All complexity properties are (nearly) ideal
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    template<typename Comparator>
    void sort(Comparator f)
    {
        tree_insertion_sort_impl<T>(f, *this, 0, size());
    }
};

template<typename TK, typename TV>
struct ListMap {
    Vec<Pair<TK, TV>> list;
    
    TV & operator[](const TK & key)
    {
        for (size_t i = 0; i < list.size(); i++)
        {
            if (list[i]._0 == key)
                return list[i]._1;
        }
        list.push_back({key, {}});
        return list.back()._1;
    }
    size_t count(const TK & key)
    {
        for (size_t i = 0; i < list.size(); i++)
        {
            if (list[i]._0 == key)
                return 1;
        }
        return 0;
    }
    void clear() { list = {}; }
    
    
    template<typename U1, typename U2>
    void insert(U1 && key, U2 && val)
    {
        list.push_back({std::forward<U1>(key), std::forward<U2>(val)});
    }
    
    Pair<TK, TV> * begin() noexcept { return list.begin(); }
    Pair<TK, TV> * end() noexcept { return list.end(); }
    const Pair<TK, TV> * begin() const noexcept { return list.begin(); }
    const Pair<TK, TV> * end() const noexcept { return list.end(); }
};

template<typename T>
struct ListSet {
    Vec<T> list;
    size_t count(const T & key)
    {
        for (size_t i = 0; i < list.size(); i++)
        {
            if (list[i] == key)
                return 1;
        }
        return 0;
    }
    void insert(T && key)
    {
        list.push_back(key);
    }
    void insert(const T & key)
    {
        list.push_back(key);
    }
    void clear() { list = {}; }
    
    T * begin() noexcept { return list.begin(); }
    T * end() noexcept { return list.end(); }
    const T * begin() const noexcept { return list.begin(); }
    const T * end() const noexcept { return list.end(); }
};

class String {
private:
    // if bytes.size() is not zero, is long string with bytes.size() - 1 number of non-null chars and 1 null terminator
    Vec<char> bytes = {};
    // if bytes.size() is 0, is shortstr with shortstr_len chars
    // (max shortstr len is 6, because seventh char must be null terminator)
    char shortstr[7] = {0, 0, 0, 0, 0, 0, 0};
    unsigned char shortstr_len = 0;
public:
    char & operator[](size_t pos) noexcept { return bytes.size() ? bytes[pos] : shortstr[pos]; }
    const char & operator[](size_t pos) const noexcept { return bytes.size() ? bytes[pos] : shortstr[pos]; }
    char * data() noexcept { return bytes.size() ? bytes.data() : shortstr; }
    const char * data() const noexcept { return bytes.size() ? bytes.data() : shortstr; }
    char * c_str() noexcept { return data(); }
    const char * c_str() const noexcept { return data(); }
    
    size_t size() const noexcept { return bytes.size() ? bytes.size() - 1 : shortstr_len; }
    size_t length() const noexcept { return size(); }
    size_t capacity() const noexcept { return bytes.size() ? bytes.capacity() - 1 : 6; }
    char * begin() noexcept { return bytes.size() ? bytes.begin() : shortstr; }
    char * end() noexcept { return bytes.size() ? bytes.end() : (shortstr + shortstr_len); }
    const char * begin() const noexcept { return bytes.size() ? bytes.begin() : shortstr; }
    const char * end() const noexcept { return bytes.size() ? bytes.end() : (shortstr + shortstr_len); }
    
    char & front() { return bytes.size() ? bytes.front() : shortstr[0]; }
    char & back() { return bytes.size() ? bytes.back() : shortstr[shortstr_len]; }
    const char & front() const { return bytes.size() ? bytes.front() : shortstr[0]; }
    const char & back() const { return bytes.size() ? bytes.back() : shortstr[shortstr_len]; }
    
    bool operator==(const String & other) const&
    {
        if (other.size() != size())
            return false;
        
        for (size_t i = 0; i < size(); i++)
        {
            if (!((*this)[i] == other[i]))
                return false;
        }
        return true;
    }
    
    String & operator+=(const String & other) &
    {
        if (bytes.size() > 0)
        {
            bytes.pop_back();
            for (size_t i = 0; i < other.size(); i++)
                bytes.push_back(other[i]);
            bytes.push_back(0);
        }
        else if (size() + other.size() < 7)
        {
            for (size_t i = 0; i < other.size(); i++)
                shortstr[shortstr_len++] = other[i];
        }
        else
        {
            for (size_t i = 0; i < shortstr_len; i++)
                bytes.push_back(shortstr[i]);
            for (size_t i = 0; i < other.size(); i++)
                bytes.push_back(other[i]);
            bytes.push_back(0);
        }
        
        return *this;
    }
    
    String & operator+=(char c) &
    {
        char chars[2];
        chars[0] = c;
        chars[1] = 0;
        return *this += String(chars);
    }
    
    String operator+(const String & other) const&
    {
        String ret;
        ret += *this;
        ret += other;
        return ret;
    }
    
    String substr(size_t pos, size_t len) const&
    {
        if (ptrdiff_t(pos) < 0)
            pos += size();
        if (ptrdiff_t(pos) < 0 || pos >= size())
            return String();
        len = len < size() - pos ? len : size() - pos;
        String ret;
        for (size_t i = 0; i < len; i++)
            ret += (*this)[pos + i];
        return ret;
    }
    
    String() noexcept { }
    String(const String & other)
    {
        bytes = other.bytes;
        memcpy(shortstr, other.shortstr, 7);
        shortstr_len = other.shortstr_len;
    }
    String(const char * data)
    {
        size_t len = 0;
        while (data[len++] != 0);
        if (len <= 7) // includes null terminator
        {
            shortstr_len = len - 1;
            memmove(shortstr, data, shortstr_len);
        }
        else
        {
            const char * end = data + len;
            bytes = Vec<char>(data, end);
        }
    }
};

/*
template <>
struct std::hash<String>
{
    size_t operator()(const String & string) const
    {
        const char * dat = string.data();
        const size_t len = string.size();
        size_t hashval = 0;
        for (size_t i = 0; i < len; i++)
        {
            hashval ^= hash<char>()(dat[i]);
            hashval *= 0xf491;
        }
        return hashval;
    }
};
*/

#endif // _INCLUDE_BXX_TYPES
