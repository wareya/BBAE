#include <stdio.h>
int main(void)
{
    volatile int _n = 1000000000;
    int n = _n;
    double sum = 0.0;
    double flip = -1.0;

    for (int i = 0; i < n; i++)
    {
        flip *= -1.0;
        sum += flip / (2.0 * i + 1.0);
    }

    printf("%.13f\n", sum * 4.0);
}