#include "TransThread.h"
#include "libcoro/coro.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main (int argc, char * argv[]) {
    unsigned int seed = time(NULL);
    if (argc >= 2) {
        seed = atoi(argv[1]);
    }
    srand(seed);
    printf("The seed used:%d\n", seed);
    for (int i = 0; i < 10; i++) printf("%d\n", rand());
    return 0;
}
