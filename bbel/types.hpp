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
};

template<typename T, class... Args>
constexpr static Shared<T> MakeShared(Args &&... args) noexcept
{
    T obj = T{std::forward<Args>(args)...};
    Shared<T> ret(std::move(obj));
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
        
        apply_cap_hint(other.capacity());
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
    constexpr T pop_back()
    {
        return erase_at(size() - 1);
    }
    constexpr void erase(const T * which)
    {
        size_t i = which - (T*)mbuffer;
        erase_at(i);
    }
    constexpr void erase(T * which)
    {
        size_t i = which - (T*)mbuffer;
        erase_at(i);
    }
    
    /// Performs merge sort with auxiliary memory.
    /// Insertion sort is a sorting algorithm with:
    /// - Downside: temporarily allocates size() extra items worth of memory
    /// - All other properties are (nearly) ideal
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    /// This is an adaptive variant that skips the beginning and end of the Vec if they're already sorted.
    template<typename Comparator>
    void merge_sort(Comparator f)
    {
        if (size() * sizeof(T) <= 128 || size() < 8)
            return insertion_sort(f);
        Vec<T> temp;
        temp.reserve(size());
        size_t start = 0;
        size_t one_past_end = size();
        shrink_sort_bounds(f, data(), start, one_past_end);
        //size_t midpoint = start + (one_past_end - start) / 2;
        //merge_sort_impl(f, data(), temp.data(), start, one_past_end, midpoint, 0);
        merge_sort_bottomup_impl(f, data(), temp.data(), start, one_past_end);
        //merge_sort_bottomup_textbook_impl(f, data(), temp.data(), start, one_past_end);
    }
    template<typename Comparator>
    void merge_sort_td(Comparator f)
    {
        if (size() * sizeof(T) <= 128 || size() < 8)
            return insertion_sort(f);
        Vec<T> temp;
        temp.reserve(size());
        size_t start = 0;
        size_t one_past_end = size();
        shrink_sort_bounds(f, data(), start, one_past_end);
        size_t midpoint = start + (one_past_end - start) / 2;
        merge_sort_impl(f, data(), temp.data(), start, one_past_end, midpoint, 0);
    }
    template<typename Comparator>
    void merge_sort_bu(Comparator f)
    {
        if (size() * sizeof(T) <= 128 || size() < 8)
            return insertion_sort(f);
        Vec<T> temp;
        temp.reserve(size());
        size_t start = 0;
        size_t one_past_end = size();
        shrink_sort_bounds(f, data(), start, one_past_end);
        merge_sort_bottomup_textbook_impl(f, data(), temp.data(), start, one_past_end);
    }
    
    /// Performs merge sort in place.
    /// Insertion sort is a sorting algorithm with:
    /// - Downside: slightly higher "constant factor" than merge_sort, except on already-mostly-sorted arrays
    /// - All other properties are (nearly) ideal
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    /// This is an adaptive variant that skips the beginning and end of the Vec if they're already sorted.
    template<typename Comparator>
    void merge_sort_inplace(Comparator f)
    {
        if (size() * sizeof(T) <= 128 || size() < 8)
            return insertion_sort(f);
        size_t start = 0;
        size_t one_past_end = size();
        shrink_sort_bounds(f, data(), start, one_past_end);
        merge_sort_inplace_impl(f, data(), start, one_past_end);
        //merge_sort_bottomup_inplace_impl(f, data(), start, one_past_end);
    }
    template<typename Comparator>
    void merge_sort_inplace_bu(Comparator f)
    {
        if (size() * sizeof(T) <= 128 || size() < 8)
            return insertion_sort(f);
        size_t start = 0;
        size_t one_past_end = size();
        shrink_sort_bounds(f, data(), start, one_past_end);
        merge_sort_bottomup_inplace_impl(f, data(), start, one_past_end);
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
        insertion_sort_impl(f, data(), 0, size());
    }
    /// Performs quicksort.
    /// quicksort is a sorting algorithm with:
    /// - Downside: "unstable" (does not retain order of equally-ordered elements).
    /// - Downside: Can take more than O(n logn) time for specially-crafted malicious inputs, but is usually ideally fast.
    /// - All other properties are (nearly) ideal
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    /// This is an adaptive sort that skips the beginning and end of the Vec if they're already sorted.
    template<typename Comparator>
    void quick_sort(Comparator f)
    {
        size_t start = 0;
        size_t one_past_end = size();
        shrink_sort_bounds(f, data(), start, one_past_end);
        quick_sort_impl(f, data(), start, one_past_end);
    }
    /// Sorts the Vec.
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    template<typename Comparator>
    void sort(Comparator f)
    {
        this->merge_sort_in_place(f);
    }
    
private:
    
    void apply_cap_hint(size_t count)
    {
        mcapacity = count > 0 ? 1 : 0;
        
        while (mcapacity < count)
            mcapacity = (mcapacity << 1) >= count ? (mcapacity << 1) : count;
        
        if (mcapacity != 0)
        {
            mbuffer_raw = (char *)malloc(mcapacity * sizeof(T) + alignof(T));
            if (!mbuffer_raw) throw;
            mbuffer = mbuffer_raw;
            while (size_t(mbuffer) % alignof(T))
                mbuffer += 1;
        }
    }
    
    //template<typename = typename std::enable_if<std::is_copy_constructible<T>::value>::type>
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
