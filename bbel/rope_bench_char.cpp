#include "types.hpp"

#include "picojson.hpp"

#include <stdint.h>
#include <cstring>

#include <chrono>
#include <ext/rope>

#define restrict
#include "rope.c"

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
    FILE * file = fopen("asdfasdf.txt", "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    size_t filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char * text = (char *)malloc(filesize + 1);
    assert(text);

    fread(text, sizeof(char), filesize, file);
    text[filesize] = 0;

    fclose(file);
    
    struct Tx { size_t start = 0; size_t erased = 0; String insert; };
    
    Vec<Tx> data;
    size_t i = 0;
    while (text[i])
    {
        size_t n = 0;
        Tx tx;
        while (text[i] != ' ') tx.start = (tx.start * 10) + (text[i++] - '0');
        i++;
        while (text[i] != ' ') tx.erased = (tx.erased * 10) + (text[i++] - '0');
        i++;
        size_t count = 0;
        while (text[i] != ' ') count = (count * 10) + (text[i++] - '0');
        i++;
        while (count--)
        {
            tx.insert += text[i];
            i += 1;
        }
        while (text[i] == '\n' || text[i] == '\r')
            i += 1;
        data.push_back(tx);
    }
    
    //Rope<char, 384/4, 384> rope;
    Rope<char, 16, 256> rope;
    //Rope<char, 32, 128> rope;
    //Rope<char, 384/4, 384> rope;
    double start = seconds();
    
    for (size_t z = 0; z < 500; z++)
    {
        rope.clear();
        for (auto & tx : data)
        {
            if (tx.erased > 0)
            {
                for (size_t i = 0; i < tx.erased; i++)
                    rope.erase_at(tx.start, true);
            }
            if (tx.insert.size() > 0)
            {
                size_t i = 0;
                for (auto c : tx.insert)
                    rope.insert_at(tx.start + (i++), c, true);
            }
        }
    }
    
    double end = seconds();
    fprintf(stderr, "rope build time: %f\n", end - start);
    
    for (auto & c : rope)
    {
        printf("%c", c);
    }
    
    /*
    Rope<char> rope2;
    __gnu_cxx::rope<char> rope_stl;
    Vec<char> vec2;
    auto lrope = rope_new();
    
    size_t count = 1000000;
    printf("n: %zd\n", count);
    
    uint64_t state = 1234567;
    double start = seconds();
    for (size_t i = 0; i < count; i++)
        //rope2.insert_at(i, i - 1500000);
        //rope2.insert_at(i, i);
        rope2.insert_at(i, rng(&state));
    //rope2.insert_at(0, 0xFFFFFFFF);
    //rope2.insert_at(count, 0);
    double end = seconds();
    printf("rope build time: %f\n", end - start);
    
    state = 1234567;
    start = seconds();
    for (size_t i = 0; i < count; i++)
        //rope_stl.insert(i, i - 1500000);
        //rope_stl.insert(i, i);
        rope_stl.insert(i, rng(&state));
    //rope_stl.insert(0, 0xFFFFFFFF);
    //rope_stl.insert(count, (uint32_t)0);
    end = seconds();
    printf("rope stl build time: %f\n", end - start);
    
    state = 1234567;
    start = seconds();
    for (size_t i = 0; i < count; i++)
        //vec2.insert_at(i, i - 1500000);
        //vec2.insert_at(i, i);
        vec2.insert_at(i, rng(&state));
    //vec2.insert_at(0, 0xFFFFFFFF);
    //vec2.insert_at(count, 0);
    end = seconds();
    
    printf("vec build time: %f\n", end - start);
    
    state = 1234567;
    start = seconds();
    for (size_t i = 0; i < count; i++)
        //vec2.insert_at(i, i - 1500000);
        //vec2.insert_at(i, i);
        rope_insert(lrope, i, (const unsigned char[2]){(unsigned char)rng(&state), 0});
    //vec2.insert_at(0, 0xFFFFFFFF);
    //vec2.insert_at(count, 0);
    end = seconds();
    
    printf("librope build time: %f\n", end - start);
    
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
    
    auto r2copy = rope2;
    
    start = seconds();
    rope2.sort([&](auto & a, auto & b) { return a < b; });
    end = seconds();
    printf("rope in-out sort time: %f\n", end - start);
    
    start = seconds();
    tree_insertion_sort_impl<uint32_t>([&](auto & a, auto & b) { return a < b; }, r2copy, 0, r2copy.size());
    end = seconds();
    printf("rope ltr sort time: %f\n", end - start);
    
    if (r2copy.size() != rope2.size()) throw;
    
    start = seconds();
    auto rope_stl2 = rope_stl;
    inout_tree_insertion_sort_impl<uint32_t>([&](auto a, auto b) { return a < b; }, rope_stl, 0, rope_stl.size());
    end = seconds();
    
    printf("stl rope sort time: %f\n", end - start);
    
    start = seconds();
    vec2.sort([&](auto a, auto b) { return a < b; });
    end = seconds();
    
    for (size_t i = 0; i < rope2.size(); i++)
    {
        if (rope2[i] != vec2[i])
        {
            puts("sort 2 failed");
            printf("%zd %d %d\n", i, rope2[i], vec2[i]);
            throw;
        }
    }
    
    for (size_t i = 0; i < r2copy.size(); i++)
    {
        if (r2copy[i] != vec2[i])
        {
            puts("sort c failed");
            printf("%zd %d %d\n", i, r2copy[i], vec2[i]);
            throw;
        }
    }
    printf("vec sort time: %f\n", end - start);
    
    printf("rope rebalances: %zd\n", rope2.num_rebalances);
    
    return 0;
    */
}
