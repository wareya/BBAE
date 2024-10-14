//#include <iterator>
//#define BXX_STDLIB_ITERATOR_INCLUDED

#include "types.hpp"

#include <stdint.h>

#include <chrono>
#include <algorithm>

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

int main(void)
{
    Rope<uint32_t> rope2;
    Vec<uint32_t> vec2;
    
    size_t count = 30000000;
    printf("n: %zd\n", count);
    
    uint64_t state = 1234567;
    double start = seconds();
    for (size_t i = 0; i < count; i++)
        //rope2.insert_at(i, i - 1500000);
        rope2.insert_at(i, i);
        //rope2.insert_at(i, rng(&state));
    //rope2.insert_at(0, 0xFFFFFFFF);
    rope2.insert_at(count, 0);
    double end = seconds();
    printf("rope build time: %f\n", end - start);
    
    state = 1234567;
    start = seconds();
    for (size_t i = 0; i < count; i++)
        //vec2.insert_at(i, i - 1500000);
        vec2.insert_at(i, i);
        //vec2.insert_at(i, rng(&state));
    //vec2.insert_at(0, 0xFFFFFFFF);
    vec2.insert_at(count, 0);
    end = seconds();
    
    printf("vec build time: %f\n", end - start);
    
    uint32_t n = 0;
    
    start = seconds();
    for (size_t i = 0; i < count; i++)
        n += rope2[i];
    end = seconds();
    
    printf("rope iterate time: %f (sum: %d)\n", end - start, n);
    
    n = 0;
    
    start = seconds();
    for (size_t i = 0; i < count; i++)
        n += vec2[i];
    end = seconds();
    
    printf("vec iterate time: %f (sum: %d)\n", end - start, n);
    
    auto r2copy = rope2;
    auto r2copy2 = rope2;
    
    start = seconds();
    rope2.sort([&](auto & a, auto & b) { return a < b; });
    end = seconds();
    printf("rope in-out sort time: %f\n", end - start);
    
    start = seconds();
    tree_insertion_sort_impl<uint32_t>([&](auto & a, auto & b) { return a < b; }, r2copy, 0, r2copy.size());
    end = seconds();
    printf("rope ltr sort time: %f\n", end - start);
    
    //start = seconds();
    //std::stable_sort(r2copy2.begin(), r2copy2.end(), [&](auto & a, auto & b) { return a < b; });
    //end = seconds();
    //printf("rope std stable sort time: %f\n", end - start);
    
    if (r2copy.size() != rope2.size()) throw;
    
    for (size_t i = 0; i < r2copy.size(); i++)
    {
        if (r2copy[i] != rope2[i])
        {
            puts("sort failed");
            printf("%zd %d %d\n", i, r2copy[i], rope2[i]);
            throw;
        }
    }
    
    start = seconds();
    vec2.sort([&](auto a, auto b) { return a < b; });
    end = seconds();
    
    printf("vec sort time: %f\n", end - start);
    
    printf("rope rebalances: %zd\n", rope2.num_rebalances);
    
    return 0;
}
