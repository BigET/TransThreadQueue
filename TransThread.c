/*
Copyright (C) 2007-2019 Eduard Timotei BUDULEA

This file is part of TransThread.h

TransThread is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Foobar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#include "TransThread.h"

#ifdef USE_CORO_TEST

void yield_read();
void yield_write();

#else

#define yield_read() 
#define yield_write() 

#endif

typedef struct QueueSector{
    int const size;
    int volatile readCursor;
    int volatile writeCursor;
    struct QueueSector * volatile nextSector;
    void * volatile items[];
} QueueSector;

void * readItem(Queue * const queue) {
    if (!queue || !queue->read) return NULL;
    yield_read();
    queue->activeRead = 1;
    yield_read();
    register void * rez = NULL;
    register QueueSector * tmpRead = queue->read;
    yield_read();
    while (tmpRead) {
        if (tmpRead->readCursor < tmpRead->writeCursor) {
            yield_read();
            rez = tmpRead->items[tmpRead->readCursor];
            yield_read();
            ++tmpRead->readCursor;
            yield_read();
            break;
        }
        yield_read();
        if (tmpRead->readCursor < tmpRead->size) break;
        yield_read();
        if (!tmpRead->nextSector) break;
        yield_read();
        queue->read = tmpRead->nextSector;
        yield_read();
        tmpRead = queue->read;
        yield_read();
    }
    yield_read();
    queue->activeRead = 0;
    yield_read();
    return rez;
}

typedef struct StackItem{
    QueueSector const * const item;
    struct StackItem const * const prevItem;
} StackItem;

bool verifyLoop(StackItem const * const si) {
    StackItem const nsi = {si->item->nextSector, si};
    for (StackItem const * i = si; i ; i = i->prevItem)
        if (i->item == nsi.item) return false;
    if (!nsi.item) return true;
    return verifyLoop(&nsi);
}

bool verify(Queue const * const queue) {
    assert(queue);
    if (!queue->writeHead && !queue->write && !queue->read) return true;
    assert(queue->read || queue->writeHead == queue->write);
    assert(queue->writeHead);
    assert(queue->write);
    assert(NULL == queue->write->nextSector);
    StackItem const end = {queue->writeHead, NULL};
    assert(verifyLoop(&end));
    assert(queue->writeHead != queue->write
            || !queue->read || queue->read == queue->write);
    register QueueSector const * qs = queue->writeHead;
    if (queue->read) {
        for (;qs != queue->read; qs = qs->nextSector)
            assert(qs->readCursor == qs->size && qs->writeCursor == qs->size
                    && qs->nextSector);
        for (; qs != queue->write; qs = qs->nextSector)
            assert(qs->writeCursor == qs->size && qs->readCursor <= qs->size
                    && qs->nextSector);
    }
    assert(qs->readCursor <= qs->writeCursor && qs->writeCursor <= qs->size);
    return true;
}

int writeItem(Queue * const queue, void * const item) {
    if (!queue) {
        errno = EINVAL;
        return -1;
    }
    yield_write();
    if (!queue->write) {
        errno = ENOMEM;
        return -1;
    }
    yield_write();
    assert(verify(queue));
    if (queue->read == queue->write) {
        yield_write();
        if (queue->write->writeCursor == queue->write->readCursor) {
            yield_write();
            queue->write->writeCursor = 0;
            yield_write();
            queue->write->readCursor = 0;
        }
    }
    yield_write();
    if (queue->write->writeCursor < queue->write->size) {
        yield_write();
        queue->write->items[queue->write->writeCursor] = item;
        yield_write();
        ++queue->write->writeCursor;
        yield_write();
        if (!queue->read) {
            yield_write();
            queue->read = queue->write;
            yield_write();
        }
        return 0;
    }
    yield_write();
    if (queue->writeHead == queue->read || queue->writeHead == queue->write) {
        errno = ENOMEM;
        return -1;
    }
    yield_write();
    register QueueSector * const tmp = queue->writeHead;
    yield_write();
    queue->writeHead = tmp->nextSector;
    yield_write();
    tmp->nextSector = NULL;
    yield_write();
    tmp->writeCursor = 0;
    yield_write();
    tmp->readCursor = 0;
    yield_write();
    tmp->items[0] = item;
    yield_write();
    tmp->writeCursor = 1;
    yield_write();
    queue->write->nextSector = tmp;
    yield_write();
    queue->write = tmp;
    yield_write();
    return 0;
}

QueueSector * recoverSector(Queue * const queue) {
    if (!queue) {
        errno = EINVAL;
        return NULL;
    }
    yield_write();
    assert(verify(queue));
    if (!queue->writeHead) return NULL;
    yield_write();
    if (!queue->read
            || (queue->read == queue->writeHead
                && queue->writeHead == queue->write
                && queue->read->readCursor == queue->read->writeCursor)) {
        yield_write();
        queue->read = NULL;
        yield_write();
        if (queue->activeRead) return NULL;
        yield_write();
        register QueueSector * const tmp = queue->writeHead;
        yield_write();
        queue->writeHead = queue->write = NULL;
        yield_write();
        return tmp;
    }
    yield_write();
    if (queue->writeHead == queue->read) return NULL;
    yield_write();
    register QueueSector * const tmp = queue->writeHead;
    yield_write();
    queue->writeHead = tmp->nextSector;
    yield_write();
    tmp->nextSector = NULL;
    yield_write();
    return tmp;
}

int submitSector(Queue * const queue, void * const mem, size_t const size) {
    if (!mem || !queue) {
        errno = EINVAL;
        return -1;
    }
    register int const tmpCount = (size - 3 * sizeof(int) - 2 * sizeof(void*)) / sizeof(void*);
    if (tmpCount <= 0) {
        errno = ENOMEM;
        return -1;
    }
    register int * const tmpSize = (int *)mem;
    tmpSize[0] = tmpCount;
    register QueueSector * const tmp = (QueueSector *)mem;
    tmp->readCursor = tmp->writeCursor = tmp->size;
    yield_write();
    tmp->nextSector = queue->writeHead;
    yield_write();
    queue->writeHead = tmp;
    yield_write();
    if (!tmp->nextSector) queue->write = tmp;
    yield_write();
    if (!queue->read) queue->read = queue->write;
    yield_write();
    return 0;
}
