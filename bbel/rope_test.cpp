#include "types.hpp"

#include <stdint.h>

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

int main(void)
{
    Rope<char> rope;
    const char * s = "Hello, world! Hello, world! Hello, world! Hello, world! Hello, world! Hello, world! Hello, world! Hello, world! Hello, world! Hello, world! Hello, world!";
    for (size_t i = 0; i < 128; i++)
        rope.insert_at(i, s[i]);
    
    for (size_t i = 0; i < 128; i++)
        printf("%c", rope[i]);
    
    puts("");
    
    const char * s2 = ">Reversify me.<";
    
    for (size_t i = 0; i < 15; i++)
        rope.insert_at(38, s2[i]);
    
    for (size_t i = 0; i < 128; i++)
        printf("%c", rope[i]);
    
    puts("");
    
    for (size_t i = 0; i < 15; i++)
        rope.erase_at(38);
    
    for (size_t i = 0; i < 128; i++)
        printf("%c", rope[i]);
    
    puts("");
    
    rope.print_structure();
    
    Rope<uint32_t> rope2;
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
        vec2.insert_at(i, state);
    end = seconds();
    
    printf("vec build time: %f\n", end - start);
    
    uint32_t n = 0;
    
    start = seconds();
    for (size_t i = 0; i < count; i++)
        n += rope2[i];
    end = seconds();
    
    printf("rope2 iterate time: %f (sum: %d)\n", end - start, n);
    
    n = 0;
    
    start = seconds();
    for (size_t i = 0; i < count; i++)
        n += vec2[i];
    end = seconds();
    
    printf("vec iterate time: %f (sum: %d)\n", end - start, n);
    
    printf("rope rebalances: %zd\n", rope2.num_rebalances);
    
    return 0;
}
