#include "TransThread.h"
#include "libcoro/coro.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

coro_context writeTask, readTask;
struct coro_stack stack;

const int theLimit = 1000000;

void yield_read() {
    if (rand() & 1) coro_transfer(&readTask, &writeTask);
}
void yield_write() {
    if (rand() & 1) coro_transfer(&writeTask, &readTask);
}

Queue queue = {NULL, NULL, NULL, 0};

void coro_readTask(void *arg) {
    int currentExpect = 1;
    int got = 0;
    do {
        got = 0;
        while (!(got = (long long int) readItem(&queue)))
            yield_read();
        assert (got == currentExpect);
        ++currentExpect;
    } while (got != theLimit);
}

typedef enum WriteCommand {
    allocateSector,
    reclaimSector,
    sendItem,
    sendItem1,
    sendItem2,
    lastCommand
} WriteCommand;

/*
We test like this.
To have a better control over the multitasking we are using cooperative multitasking.
We will have 2 co routines.
One will be the write task. The other will be the read task.
We will write from 1 up to "theLimit" and we will expect to read them in the same order.

We will do this by using a pseudo random dice to dictate what a task should do.
Switching to the other task is one of the possibility.

To prime this we will also use the dice to define a pool of sectors and the
size of them.
*/
int main (int argc, char * argv[]) {
    unsigned int seed = time(NULL);
    int currentWrite = 1;
    if (argc >= 2) {
        seed = atoi(argv[1]);
    }
    srand(seed);
    printf("The seed used:%d\n", seed);
    fflush(stdout);
    /* we have up to 100 sectors */
    int sectorNum = 1 + rand() % 100;
    int sectorStack = sectorNum - 1;
    int proc = -1;
    void **sectorPool = alloca(sizeof (void*) * sectorNum);
    int* sectorSizes = alloca(sizeof (int) * sectorNum);
    /* we make the sectors at least 1 item wide and up to 1000 items */
    for (int i = 0; i < sectorNum; ++i)
        sectorPool[i] = alloca (sectorSizes[i] =
                3 * sizeof(int) + 2 * sizeof(void*)
                + rand() % 1000 * sizeof(void*));
    coro_create(&writeTask, NULL, NULL, NULL, 0);
    coro_stack_alloc(&stack, 0);
    coro_create(&readTask, coro_readTask, NULL, stack.sptr, stack.ssze);
    do {
        if (currentWrite * 100 / theLimit != proc) {
            proc = currentWrite * 100 / theLimit;
            printf("\r%d%%", proc);
            fflush(stdout);
        }
        int nd = rand();
        WriteCommand command = nd % lastCommand;
        if (currentWrite == theLimit) command = reclaimSector;
        switch(command) {
        case allocateSector:
            if (sectorStack > 0) {
                if (0 == submitSector(&queue, sectorPool[sectorStack - 1], sectorSizes[sectorStack - 1]))
                    --sectorStack;
            }
            break;
        case reclaimSector:
            {
                void * recovered = NULL;
                if (recovered = recoverSector(&queue)) {
                    int recovIndex;
                    for (recovIndex = sectorStack;
                            recovIndex < sectorNum && recovered != sectorPool[recovIndex];
                            ++ recovIndex);
                    assert (recovIndex < sectorNum);
                    void *tmp = sectorPool[sectorStack];
                    int tmpSz = sectorSizes[sectorStack];
                    sectorPool[sectorStack] = sectorPool[recovIndex];
                    sectorSizes[sectorStack] = sectorSizes[recovIndex];
                    sectorPool[recovIndex] = tmp;
                    sectorSizes[recovIndex] = tmpSz;
                    ++sectorStack;
                }
            }
            break;
        case sendItem:
        case sendItem1:
        case sendItem2:
            if (currentWrite < theLimit && 0 == writeItem(&queue, (void*)(long int)currentWrite)) {
                ++currentWrite;
            }
            break;
        }
        yield_write();
    } while (currentWrite < theLimit || queue.write);
    return 0;
}
