#ifndef _INCLUDE_BXX_TYPES
#define _INCLUDE_BXX_TYPES

#include <cstdlib> // size_t, malloc/free
#include <cstring> // memcpy, memmove
#include <cstddef> // nullptr_t

#include <utility> // std::move, std::forward
#include <initializer_list>
#include <new>

//void asdf() { std::vector<float> asdf; }

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
    
    /// Performs insertion sort.
    /// The given f is a function that returns 1 for less-than and 0 otherwise.
    template<typename Comparator>
    void sort(Comparator f)
    {
        for(size_t i = 1; i < size(); i += 1)
        {
            T item = std::move((*this)[i]);
            size_t j;
            for(j = i; j > 0 && f(item, (*this)[j - 1]); j -= 1)
                (*this)[j] = std::move((*this)[j - 1]);
            (*this)[j] = std::move(item);
        }
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
    
    void insert(TK && key, TV && val)
    {
        list.push_back({key, val});
    }
    void insert(const TK & key, TV && val)
    {
        list.push_back({key, val});
    }
    void insert(TK && key, const TV & val)
    {
        list.push_back({key, val});
    }
    void insert(const TK & key, const TV & val)
    {
        list.push_back({key, val});
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
