#include "types.hpp"

#include "picojson.hpp"

#include <stdint.h>

#include <chrono>
#include <ext/rope>

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
    __gnu_cxx::rope<uint32_t> rope_stl;
    Vec<uint32_t> vec2;
    
    size_t count = 10000000;
    printf("n: %zd\n", count);
    
    uint64_t state = 1234567;
    double start = seconds();
    for (size_t i = 0; i < count; i++)
        rope2.insert_at(i, state);
    double end = seconds();
    printf("rope build time: %f\n", end - start);
    
    state = 1234567;
    start = seconds();
    for (size_t i = 0; i < count; i++)
        rope_stl.insert(i, state);
    end = seconds();
    printf("rope stl build time: %f\n", end - start);
    
    state = 1234567;
    start = seconds();
    for (size_t i = 0; i < count; i++)
        vec2.insert_at(i, state);
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
        n += rope_stl[i];
    end = seconds();
    
    printf("rope stl iterate time: %f (sum: %d)\n", end - start, n);
    n = 0;
    
    start = seconds();
    for (auto v = rope_stl.begin(); v < rope_stl.end(); v++)
        n += *v;
    end = seconds();
    
    printf("rope stl iterate time (iterator): %f (sum: %d)\n", end - start, n);
    
    n = 0;
    
    start = seconds();
    for (size_t i = 0; i < count; i++)
        n += vec2[i];
    end = seconds();
    
    printf("vec iterate time: %f (sum: %d)\n", end - start, n);
    
    start = seconds();
    tree_insertion_sort_impl<uint32_t>([&](auto & a, auto & b) { return a < b; }, rope2, 0, rope2.size());
    end = seconds();
    
    printf("rope sort time: %f\n", end - start);
    
    start = seconds();
    auto rope_stl2 = rope_stl;
    tree_insertion_sort_impl<uint32_t>([&](auto a, auto b) { return a < b; }, rope_stl, 0, rope_stl.size());
    end = seconds();
    
    printf("stl rope sort time: %f\n", end - start);
    
    start = seconds();
    vec2.sort([&](auto a, auto b) { return a < b; });
    end = seconds();
    
    printf("vec sort time: %f\n", end - start);
    
    printf("rope rebalances: %zd\n", rope2.num_rebalances);
    
    return 0;
}
