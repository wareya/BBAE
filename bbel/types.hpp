#ifndef _INCLUDE_BXX_TYPES
#define _INCLUDE_BXX_TYPES

#include <cstdlib> // size_t, malloc/free
#include <utility> // std::move
#include <initializer_list>

#include <cstdio>

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
    
    constexpr Option(T && item)
    {
        ::new((void*)(T*)data) T(std::move(item));
        isok = true;
    }
    
    constexpr Option(const T & item)
    {
        ::new((void*)(T*)data) T(item);
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

template<typename T1, typename T2>
class Twople {
    T1 _1;
    T2 _2;
public:
    Twople(T1 a, T2 b) : _1(a), _2(b) { }
};

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
        other.mlength = 0;
        other.mcapacity = 0;
        other.mbuffer = nullptr;
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
        if (mbuffer)
            free(mbuffer);
    }
    
    // Copy
    
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
        other.mlength = 0;
        other.mcapacity = 0;
        other.mbuffer = nullptr;
        
        return *this;
    }
    
    // API
    
    constexpr void reserve(size_t new_cap)
    {
        if (new_cap > mlength)
            do_realloc(new_cap);
    }
    
    constexpr void push_back(T item)
    {
        if (mlength >= mcapacity)
        {
            mcapacity = mcapacity == 0 ? 1 : (mcapacity << 1);
            do_realloc(mcapacity);
        }
        ::new((void*)(((T*)mbuffer) + mlength)) T(std::move(item));
        mlength += 1;
    }
    constexpr T pop_back()
    {
        if (mlength == 0)
            throw;
        
        T ret(std::move(((T*)mbuffer)[mlength - 1]));
        
        ((T*)mbuffer)[mlength - 1].~T();
        mlength -= 1;
        maybe_shrink();
        
        return ret;
    }
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
    
    constexpr bool operator==(const String & other) const&
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

#endif // _INCLUDE_BXX_TYPES
