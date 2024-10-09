
#include "types.hpp"

#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <string>

#include <chrono>

double seconds()
{
    auto t = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::chrono::duration<double>(t).count();
}

uint32_t rng(uint64_t * state)
{
    const uint64_t a = 0x4b76db55;
    const uint64_t c = 1;
    *state = (*state + c) * a;
    return (*state >> 32);
}


size_t movecount = 0;
size_t copycount = 0;
struct I {
    uint32_t x;
    
    I(uint32_t val)
    {
        x = val;
    }
    I & operator=(uint32_t val)
    {
        x = val;
        return *this;
    }
    
    I(I && item)
    {
        //movecount += 1;
        x = item.x;
    }
    
    explicit I(const I & item)
    {
        x = item.x;
    }
    
    I & operator=(I && item)
    {
        //movecount += 1;
        x = item.x;
        return *this;
    }
    I & operator=(const I & item)
    {
        //copycount += 1;
        x = item.x;
        return *this;
    }
};

    
template<typename T>
void comparify(Vec<T> & vec)
{
    Vec<T> vec2 = vec;
    double time = 0.0;
    for (size_t i = 0; i < 5; i++)
    {
        vec2 = vec;
        double start = seconds();
        vec2.merge_sort_bu([](auto & a, auto & b) { return a < b; });
        double end = seconds();
        time += (end - start) / 5.0;
    }
    printf(", %f", time);
    
    Vec<T> vec3 = vec;
    time = 0.0;
    for (size_t i = 0; i < 5; i++)
    {
        vec3 = vec;
        double start = seconds();
        vec3.merge_sort([](auto & a, auto & b) { return a < b; });
        double end = seconds();
        time += (end - start) / 5.0;
    }
    printf(", %f", time);
    
    Vec<T> vec4 = vec;
    time = 0.0;
    for (size_t i = 0; i < 5; i++)
    {
        vec4 = vec;
        double start = seconds();
        vec4.merge_sort_td([](auto & a, auto & b) { return a < b; });
        double end = seconds();
        time += (end - start) / 5.0;
    }
    printf(", %f", time);
    
    Vec<T> vec5 = vec;
    time = 0.0;
    for (size_t i = 0; i < 5; i++)
    {
        vec5 = vec;
        double start = seconds();
        std::stable_sort(vec5.begin(), vec5.end(), [](auto & a, auto & b) { return a < b; });
        double end = seconds();
        time += (end - start) / 5.0;
    }
    printf(", %f", time);
    
    Vec<T> vec6 = vec;
    time = 0.0;
    for (size_t i = 0; i < 5; i++)
    {
        vec6 = vec;
        double start = seconds();
        std::sort(vec6.begin(), vec6.end(), [](auto & a, auto & b) { return a < b; });
        double end = seconds();
        time += (end - start) / 5.0;
    }
    printf(", %f", time);
    
    Vec<T> vec7 = vec;
    time = 0.0;
    for (size_t i = 0; i < 5; i++)
    {
        vec7 = vec;
        double start = seconds();
        vec7.merge_sort_inplace([](auto & a, auto & b) { return a < b; });
        double end = seconds();
        time += (end - start) / 5.0;
    }
    printf(", %f", time);
    
    Vec<T> vecF = vec;
    time = 0.0;
    for (size_t i = 0; i < 5; i++)
    {
        vecF = vec;
        double start = seconds();
        vecF.merge_sort_inplace_bu([](auto & a, auto & b) { return a < b; });
        double end = seconds();
        time += (end - start) / 5.0;
    }
    printf(", %f", time);
    
    Vec<T> vec8 = vec;
    time = 0.0;
    for (size_t i = 0; i < 5; i++)
    {
        vec8 = vec;
        double start = seconds();
        vec8.quick_sort([](auto & a, auto & b) { return a < b; });
        double end = seconds();
        time += (end - start) / 5.0;
    }
    printf(", %f", time);
    
    for (size_t i = 0; i < vec.size(); i++)
    {
        assert(vec2[i] == vec5[i]);
        assert(vec3[i] == vec5[i]);
        assert(vec4[i] == vec5[i]);
        assert(vec7[i] == vec5[i]);
    }
}
    
int main(void)
{
    uint64_t state  = 6153643146;
    uint64_t state2 = 6153643149;
    Vec<std::string> vec;
    //Vec<std::string> vec;
    //Vec<double> vec;
    //for (size_t i = 0; i < 400000; i++)
    //for (size_t i = 0; i < (1<<20); i++)
    //  vec.push_back(rng(&state));
    
    uint32_t n = 0;
    uint32_t n2 = 0;
    //for (size_t i = 0; i < (1<<14); i++)
    //for (size_t i = 0; i < 405378; i++)
    //for (size_t i = 0; i < 51173; i++)
    //for (size_t i = 0; i < 8196; i++)
    //for (size_t i = 0; i < 4096; i++)
    
    //for (size_t i = 0; i < 511731; i++)
    for (size_t i = 0; i < 217311; i++)
    {
        //vec.push_back((i % 2) * 10000 - i / 2);
        //vec.push_back(i / 100 + (i % 214));
        //vec.push_back((i % 214) - i / 100);
        //vec.push_back(i / 100 - (i % 214) + 214);
        //vec.push_back((n  += (rng(&state) >> 19)) + i);
        //vec.push_back((n2 += (rng(&state2) >> 30)) + i);
        //vec.push_back(100 + (rng(&state) >> 27));
        //vec.push_back(0 + (rng(&state2) >> 27));
        //vec.push_back(rng(&state2) / 10000);
        //vec.push_back(rng(&state2));
        //vec.push_back((rng(&state2) & 0xFFFF) + ((uint64_t)1 << (rng(&state)%30)));
        //vec.push_back((1<<25) - 12500000 - i);
        //vec.push_back((1<<25) - 12500000 - i + rng(&state2) / 10000);
        //vec.push_back((rng(&state2) % 100) + i / 50000 + rng(&state2) % 50);
        //vec.push_back((i * 20 - 10) % 100 + ((10-i) * 20) / 100 * 100);
        
        vec.push_back(std::to_string(rng(&state2) / 10000));
    }
    //for (size_t i = 0; i < 511731; i++)
    //    vec.push_back((n2 += (rng(&state2) >> 19)) + i);
    
    for (size_t i = 0; i < vec.size(); i += vec.size()/10)
        printf("%s\n", vec[i].data());
        //printf("%f\n", vec[i]);
        //printf("%u\n", vec[i]);
    
    auto vec2 = vec;
    
    double start = seconds();
    //vec.merge_sort_inplace([](auto a, auto b) { return a < b; });
    //vec.quick_sort([](auto a, auto b) { return a / 100 < b / 100; });
    //vec.merge_sort([](auto & a, auto & b) { return a.x < b.x; });
    //std::stable_sort(vec.begin(), vec.end(), [](auto & a, auto & b) { return a.x < b.x; });
    vec.quick_sort([](auto & a, auto & b) { return a < b; });
    //vec.merge_sort([](auto & a, auto & b) { return a < b; });
    //vec.merge_sort_inplace([](auto a, auto b) { return a / 100 < b / 100; });
    //vec.merge_sort_inplace([](auto a, auto b) { return a < b; });
    //std::stable_sort(vec.begin(), vec.end(), [](auto a, auto b) { return a < b; });
    //std::stable_sort(vec.begin(), vec.end(), [](auto a, auto b) { return a / 100 < b / 100; });
    //std::sort(vec.begin(), vec.end(), [](auto a, auto b) { return a / 100 < b / 100; });
    //vec.insertion_sort([](auto a, auto b) { return a / 100 < b / 100; });
    double end = seconds();
    
    puts("");
    for (size_t i = 0; i < vec.size(); i += vec.size()/10)
        printf("%s\n", vec[i].data());
        //printf("%f\n", vec[i]);
        //printf("%u\n", vec[i]);
    
    printf("%f seconds\n", end - start);
    
    for (size_t i = 0; i < 5; i++)
    {
        vec = vec2;
        movecount = 0;
        double start = seconds();
        
        //size_t cmps = 0;
        
        //vec.merge_sort([&](auto a, auto b) { cmps += 1; return a < b; });
        //vec.merge_sort_inplace([&](auto a, auto b) { cmps += 1; return a < b; });
        //std::stable_sort(vec.begin(), vec.end(), [&](auto a, auto b) { cmps += 1; return a < b; });
        //vec.quick_sort([](auto a, auto b) { return a < b; });
        //vec.merge_sort([&](auto & a, auto & b) { cmps += 1; return a < b; });
        //vec.merge_sort([](auto & a, auto & b) { return a < b; });
        //vec.merge_sort_inplace([&](auto & a, auto & b) { cmps += 1; return a.x < b.x; });
        //vec.merge_sort_inplace([&](auto & a, auto & b) { return a < b; });
        //vec.merge_sort([](auto & a, auto & b) { return a.x < b.x; });
        vec.merge_sort([](auto & a, auto & b) { return a < b; });
        //vec.merge_sort_inplace([](auto a, auto b) { return a < b; });
        //vec.merge_sort([&](auto & a, auto & b) { cmps += 1; return a.x < b.x; });
        //std::stable_sort(vec.begin(), vec.end(), [&](auto & a, auto & b) { cmps += 1; return a.x < b.x; });
        //std::stable_sort(vec.begin(), vec.end(), [&](auto & a, auto & b) { cmps += 1; return a < b; });
        //std::stable_sort(vec.begin(), vec.end(), [](auto a, auto b) { return a < b; });
        //std::stable_sort(vec.begin(), vec.end(), [](auto & a, auto & b) { return a.x < b.x; });
        //std::sort(vec.begin(), vec.end(), [](auto a, auto b) { return a < b; });
        
        //vec.quick_sort([](auto a, auto b) { return a / 100 < b / 100; });
        //vec.merge_sort([](auto a, auto b) { return a / 100 < b / 100; });
        //vec.merge_sort_inplace([](auto a, auto b) { return a / 100 < b / 100; });
        //std::stable_sort(vec.begin(), vec.end(), [](auto a, auto b) { return a / 100 < b / 100; });
        //std::sort(vec.begin(), vec.end(), [](auto a, auto b) { return a / 100 < b / 100; });
        
        double end = seconds();
        //printf("comparisons: %zu\n", cmps);
        //printf("moves: %zu\n", movecount);
        //printf("copies: %zu\n", copycount);
        printf("%f\n", end - start);
    }
    
    printf("data, textbook bu, coherent bu, td, std::stable_sort, std::sort, inplace td, inplace bu, quicksort\n");
    
    state  = 6153643146;
    state2 = 6153643149;
    n = 0;
    n2 = 0;
    Vec<uint32_t> forwards;
    for (size_t i = 0; i < 15117311; i++)
        forwards.push_back((n  += (rng(&state) >> 28)) + i);
    printf("presorted");
    comparify(forwards);
    
    state2 = 6153643149;
    Vec<std::string> strings;
    for (size_t i = 0; i < 2173111; i++)
        strings.push_back(std::to_string(rng(&state2)));
    printf("strings");
    comparify(strings);
    
    state2 = 6153643149;
    strings = {};
    for (size_t i = 0; i < 2173111; i++)
        strings.push_back(std::to_string(rng(&state2) / 1000000));
    printf("strings (many duplicates)");
    comparify(strings);
    
    state2 = 6153643149;
    Vec<uint32_t> white;
    for (size_t i = 0; i < 5117311; i++)
        white.push_back(rng(&state2));
    printf("white noise");
    comparify(white);
    
    state2 = 6153643149;
    Vec<uint16_t> white2;
    for (size_t i = 0; i < 5117311; i++)
        white2.push_back(rng(&state2));
    printf("white noise (u16)");
    comparify(white2);
    
    state2 = 6153643149;
    Vec<uint8_t> white3;
    for (size_t i = 0; i < 5117311; i++)
        white3.push_back(rng(&state2));
    printf("white noise (u8)");
    comparify(white3);
    
    state2 = 6153643149;
    Vec<uint64_t> white4;
    for (size_t i = 0; i < 5117311; i++)
        white4.push_back((rng(&state2), state2));
    printf("white noise (u64)");
    comparify(white4);
    
    forwards.insert_at(0, 0xFFFFFFFF);
    printf("almost presorted");
    comparify(forwards);
    
    state2 = 6153643149;
    Vec<uint32_t> bruh;
    for (size_t i = 0; i < 15117311; i++)
        bruh.push_back(((rng(&state2) >> 24) & 1) * 1000);
    printf("two values");
    comparify(bruh);
    
    Vec<uint32_t> rev;
    for (size_t i = 0; i < 15117311; i++)
        rev.push_back(15117311 - i);
    printf("reversed");
    comparify(rev);
    
    rev = {};
    for (size_t i = 0; i < 15117311; i++)
        rev.push_back(5117311 - i);
    printf("broken reversed");
    comparify(rev);
    
    rev = {};
    for (size_t i = 0; i < 15117311; i++)
        rev.push_back((15117311 - i) / 10000);
    printf("reversed (many duplicates)");
    comparify(rev);
    
    state2 = 6153643149;
    rev = {};
    for (size_t i = 0; i < 5117311; i++)
        rev.push_back((1<<25) - 12500000 - i + rng(&state2) / 10000);
    printf("reversed noisy");
    comparify(rev);
    
    rev = {};
    for (size_t i = 0; i < 15117311; i++)
        rev.push_back((i % 2) * 10000 - i / 2);
    printf("sawtooth A");
    comparify(rev);
    
    rev = {};
    for (size_t i = 0; i < 15117311; i++)
        rev.push_back(i / 100 + (i % 214));
    printf("sawtooth B");
    comparify(rev);
    
    rev = {};
    for (size_t i = 0; i < 15117311; i++)
        rev.push_back((i % 214) - i / 100);
    printf("sawtooth C");
    comparify(rev);
    
    rev = {};
    for (size_t i = 0; i < 15117311; i++)
        rev.push_back(i / 100 - (i % 214) + 214);
    printf("sawtooth D");
    comparify(rev);
    
    state2 = 6153643149;
    
    white = {};
    for (size_t i = 0; i < 5117311; i++)
        white.push_back((rng(&state2) % 100) + i / 50000 + rng(&state2) % 50);
    printf("many duplicates A");
    comparify(white);
    
    white = {};
    for (size_t i = 0; i < 5117311; i++)
        white.push_back(rng(&state2) / 10000);
    printf("many duplicates B");
    comparify(white);
    
    state  = 6153643146;
    state2 = 6153643149;
    n = 0;
    n2 = 0;
    Vec<uint32_t> crap;
    for (size_t i = 0; i < 511731; i++)
    {
        crap.push_back((i % 2) * 10000 - i / 2);
        crap.push_back(i / 100 + (i % 214));
        crap.push_back((i % 214) - i / 100);
        crap.push_back(i / 100 - (i % 214) + 214);
        crap.push_back((n  += (rng(&state) >> 19)) + i);
        crap.push_back((n2 += (rng(&state2) >> 30)) + i);
        crap.push_back(100 + (rng(&state) >> 27));
        crap.push_back(0 + (rng(&state2) >> 27));
        crap.push_back(rng(&state2) / 10000);
        crap.push_back(rng(&state2));
        crap.push_back((rng(&state2) & 0xFFFF) + ((uint64_t)1 << (rng(&state)%30)));
        crap.push_back((1<<25) - 12500000 - i);
        crap.push_back((1<<25) - 12500000 - i + rng(&state2) / 10000);
        crap.push_back((rng(&state2) % 100) + i / 50000 + rng(&state2) % 50);
        crap.push_back((i * 20 - 10) % 100 + ((10-i) * 20) / 100 * 100);
    }
    for (size_t i = 0; i < 511731; i++)
        crap.push_back((n2 += (rng(&state2) >> 19)) + i);
    printf("a bunch of crap");
    comparify(crap);
    
    state  = 6153643146;
    state2 = 6153643149;
    n = 0;
    n2 = 0;
    Vec<uint32_t> concat;
    for (size_t i = 0; i < 5117311; i++)
    {
        concat.push_back((n  += (rng(&state) >> 19)) + i);
        concat.push_back((n2 += (rng(&state2) >> 19)) + i);
    }
    printf("appended sorted arrays");
    comparify(concat);
    
    state  = 6153643146;
    state2 = 6153643149;
    n = 0;
    n2 = 0;
    Vec<uint32_t> interleave;
    for (size_t i = 0; i < 5117311; i++)
        interleave.push_back((n  += (rng(&state) >> 19)) + i);
    for (size_t i = 0; i < 5117311; i++)
        interleave.push_back((n2 += (rng(&state2) >> 19)) + i);
    printf("interleaved sorted arrays");
    comparify(interleave);
    
    puts("all done!");
    
    return 0;
}
