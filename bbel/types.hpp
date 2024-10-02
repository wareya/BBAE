#ifndef _INCLUDE_BXX_TYPES
#define _INCLUDE_BXX_TYPES

#include <cstdlib> // size_t, malloc/free
#include <utility> // std::move
#include <initializer_list>

#include <cstdio>

template<typename T>
class Vec {
private:
    size_t mlength = 0;
    size_t mcapacity = 0;
    char * mbuffer = nullptr;
public:
    size_t size() const noexcept { return mlength; }
    size_t capacity() const noexcept { return mcapacity; }
    T * data() noexcept { return (T*)mbuffer; }
    const T * data() const noexcept { return (T*)mbuffer; }
    T * begin() const noexcept { return (T*)mbuffer; }
    T * end() const noexcept { return ((T*)mbuffer) + mlength; }
    
    T & front() const { if (mlength == 0) throw; return ((T*)mbuffer)[0]; }
    T & back() const { if (mlength == 0) throw; return ((T*)mbuffer)[mlength - 1]; }
    
    const T & operator[](size_t pos) const noexcept { return ((T*)mbuffer)[pos]; }
    T & operator[](size_t pos) noexcept { return ((T*)mbuffer)[pos]; }
    
    // Constructors
    
    // Blank
    
    Vec() noexcept { }
    
    // With capacity hint
    
    explicit Vec(size_t count)
    {
        apply_cap_hint(count);
    }
    
    // Filled with default
    
    Vec(size_t count, T default_val)
    {
        *this = Vec(count);
        mlength = count;
        
        for (size_t i = 0; i < mlength; i++)
            ::new((void*)(((T*)mbuffer) + i)) T(default_val);
    }
    
    // Copy
    
    Vec(const Vec & other)
    {
        apply_cap_hint(other.capacity());
        copy_into(*this, other);
    }
    Vec(Vec && other)
    {
        mlength = other.mlength;
        mcapacity = other.mcapacity;
        mbuffer = other.mbuffer;
        other.mlength = 0;
        other.mcapacity = 0;
        other.mbuffer = nullptr;
    }
    template<class Iterator>
    Vec(Iterator first, Iterator past_end)
    {
        *this = Vec();
        while (first != past_end)
        {
            push_back(*first);
            first++;
        }
    }
    Vec(std::initializer_list<T> initializer)
    {
        *this = Vec(initializer.size());
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
        if (mbuffer)
            free(mbuffer);
    }
    
    // Copy
    
    Vec & operator=(const Vec & other)
    {
        if (this == &other)
            return *this;
        
        apply_cap_hint(other.capacity());
        copy_into(*this, other);
        
        return *this;
    }
    
    // API
    
    void push_back(T item)
    {
        if (mlength >= mcapacity)
        {
            mcapacity = mcapacity == 0 ? 1 : (mcapacity << 1);
            do_realloc(mcapacity);
        }
        ::new((void*)(((T*)mbuffer) + mlength)) T(std::move(item));
        mlength += 1;
    }
    T pop_back()
    {
        if (mlength == 0)
            throw;
        
        T ret(std::move(((T*)mbuffer)[mlength - 1]));
        
        ((T*)mbuffer)[mlength - 1].~T();
        mlength -= 1;
        maybe_shrink();
        
        return ret;
    }
    T erase_at(size_t i)
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
    void erase(const T * which)
    {
        size_t i = which - (T*)mbuffer;
        erase_at(i);
    }
    void erase(T * which)
    {
        size_t i = which - (T*)mbuffer;
        erase_at(i);
    }
    
private:
    
    void apply_cap_hint(size_t count)
    {
        mcapacity = count > 0 ? 1 : 0;
        
        while (mcapacity < count)
            mcapacity = (mcapacity << 1) >= count ? (mcapacity << 1) : count;
        
        if (mcapacity != 0)
        {
            mbuffer = (char *)malloc(mcapacity * sizeof(T));
            if (!mbuffer) throw;
        }
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
        char * new_buf = mcapacity ? (char *)malloc(mcapacity * sizeof(T)) : nullptr;
        if (mcapacity && !new_buf) throw;
        for (size_t i = 0; i < mlength; i++)
        {
            ::new((void*)(((T*)new_buf) + i)) T(std::move(((T*)mbuffer)[i]));
            ((T*)mbuffer)[i].~T();
        }
        if (mbuffer) free(mbuffer);
        mbuffer = new_buf;
    }
};

class String {
private:
    Vec<char> bytes;
public:
    char & operator[](size_t pos) noexcept { return bytes[pos]; }
    const char & operator[](size_t pos) const noexcept { return bytes[pos]; }
    char * data() noexcept { return bytes.data(); }
    const char * data() const noexcept { return bytes.data(); }
    
    String() : bytes() { }
    String(const char * data) : bytes()
    {
        size_t len = 0;
        while (data[len++] != 0);
        const char * end = data + len;
        bytes = Vec<char>(data, end);
    }
};

#endif // _INCLUDE_BXX_TYPES
