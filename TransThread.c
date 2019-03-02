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
#include <stddef.h>

#include "TransThread.h"

typedef struct QueueSector{
    int const size;
    int volatile readCursor;
    int volatile writeCursor;
    struct QueueSector * volatile nextSector;
    void * volatile items[];
} QueueSector;

void * readItem(Queue * const queue) {
    if (!queue || !queue->read) return NULL;
    queue->activeRead = 1;
    register void * rez = NULL;
    register QueueSector * tmpRead = queue->read;
    while (tmpRead) {
        if (tmpRead->readCursor < tmpRead->writeCursor) {
            rez = tmpRead->items[tmpRead->readCursor];
            ++tmpRead->readCursor;
            break;
        }
        if (tmpRead->readCursor < tmpRead->size) break;
        if (!tmpRead->nextSector) break;
        queue->read = tmpRead->nextSector;
        tmpRead = queue->read;
    }
    queue->activeRead = 0;
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
    if (!queue) return false;
    if (!queue->writeHead && !queue->write && !queue->read) return true;
    if (!queue->read && queue->writeHead != queue->write) return false;
    if (!queue->writeHead) return false;
    if (!queue->write) return false;
    if (queue->write->nextSector) return false;
    StackItem const end = {queue->writeHead, NULL};
    if (!verifyLoop(&end)) return false;
    if (queue->writeHead == queue->write
            && queue->read && queue->read != queue->write) return false;
    register QueueSector const * qs = queue->writeHead;
    if (queue->read) {
        for (;qs != queue->read; qs = qs->nextSector)
            if (qs->readCursor != qs->size || qs->writeCursor != qs->size
                    || !qs->nextSector)
                return false;
        for (; qs != queue->write; qs = qs->nextSector)
            if (qs->writeCursor != qs->size || qs->readCursor > qs->size
                    || !qs->nextSector)
                return false;
    }
    return qs->readCursor <= qs->writeCursor && qs->writeCursor <= qs->size;
}

int writeItem(Queue * const queue, void * const item) {
    if (!queue) {
        errno = EINVAL;
        return -1;
    }
    if (!queue->write) {
        errno = ENOMEM;
        return -1;
    }
    assert(verify(queue));
    if (queue->read == queue->write) {
        if (queue->write->writeCursor == queue->write->readCursor) {
            queue->write->writeCursor = 0;
            queue->write->readCursor = 0;
        }
    }
    if (queue->write->writeCursor < queue->write->size) {
        queue->write->items[queue->write->writeCursor] = item;
        ++queue->write->writeCursor;
        if (!queue->read) {
            queue->read = queue->write;
        }
        return 0;
    }
    if (queue->writeHead == queue->read || queue->writeHead == queue->write) {
        errno = ENOMEM;
        return -1;
    }
    register QueueSector * const tmp = queue->writeHead;
    queue->writeHead = tmp->nextSector;
    tmp->nextSector = NULL;
    tmp->writeCursor = 0;
    tmp->readCursor = 0;
    tmp->items[0] = item;
    tmp->writeCursor = 1;
    queue->write->nextSector = tmp;
    queue->write = tmp;
    return 0;
}

QueueSector * recoverSector(Queue * const queue) {
    if (!queue) {
        errno = EINVAL;
        return NULL;
    }
    assert(verify(queue));
    if (!queue->writeHead) return NULL;
    if (!queue->read
            || (queue->read == queue->writeHead
                && queue->writeHead == queue->write
                && queue->read->readCursor == queue->read->writeCursor)) {
        queue->read = NULL;
        if (queue->activeRead) return NULL;
        register QueueSector * const tmp = queue->writeHead;
        queue->writeHead = queue->write = NULL;
        return tmp;
    }
    if (queue->writeHead == queue->read) return NULL;
    register QueueSector * const tmp = queue->writeHead;
    queue->writeHead = tmp->nextSector;
    tmp->nextSector = NULL;
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
    tmp->nextSector = queue->writeHead;
    queue->writeHead = tmp;
    if (!tmp->nextSector) queue->write = tmp;
    if (!queue->read) queue->read = queue->write;
    return 0;
}
