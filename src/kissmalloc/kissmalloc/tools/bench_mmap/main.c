#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const int n = 3;
    void *p[n];

    for (int i = 0; i < n; ++i) {
        // p[i] = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
        p[i] = malloc(4096);
    }

    for (int i = 0; i < n; ++i) {
        if (i > 0)
            printf("dp = %ld\n", p[i] - p[i - 1]);
        printf("p[%d] = %p\n", i, p[i]);
    }

    return 0;
}
