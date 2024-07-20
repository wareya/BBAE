#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

double mul_delta(double x)
{
    return x * 0.001;
}

int main()
{
    double y = 0.0;
    double vel = 0.0;
    double gravity = 9.8;
    for (size_t i = 1000000000; i > 0; i--)
    {
        vel += mul_delta(gravity) * 0.5;
        y += mul_delta(vel);
        vel += mul_delta(gravity) * 0.5;
    }
    printf("%f\n", y);
}