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

#ifndef TRANS_THREAD_H
#define TRANS_THREAD_H

/** An opaque pointer to internal structures. */
struct QueueSector;

/**
 * The glorious trans thread queue.
 * It is touched by the read and the write thread.
 *
 * The queue can be empty, this means that reads will yield NULLs and will not
 * mutate the queue.
 *
 * The queue can be full, this means that the writes will fail with ENOMEM,
 * it will not mutate the queue and you have to 'submitSector' to have more space.
 *
 * The queue can be in normal operation.
 *
 * If the queue has only one sector inside it will not be able to recycle
 * sectors, it needs at least 2.
 *
 * If the queue has 0 or 1 sectors submitted it will end up into an empty and
 * full state on the same time.
 *
 * At creation the queue is with 0 sectors so it will be empty and full.
 * 
 */
typedef struct Queue {
    /**
     * The head of the writing queue.
     * This member is handled only by the write thread.
     */
    struct QueueSector * volatile writeHead;
    /**
     * The tail of the writing queue.
     * This member is handled only by the write thread.
     */
    struct QueueSector * volatile write;
    /**
     * The read cursor.
     * This member is handled by both read and write thread as follows:
     *  - if it is NULL: it can be written by the write thread and only read by the read thread.
     *  - if it is not NULL and the queue is not semantically empty: it can be written by the read thread and only read by the write thread.
     *  - if it is not NULL and the queue is semantically empty: it can put to NULL by the write thread and only read by the read thread.
     */
    struct QueueSector * volatile read;
} Queue;

/** Creates of an empty queue. */
static inline Queue mkQueue() {
    Queue const tmp = {NULL, NULL, NULL};
    return tmp;
}

/**
 * Reads an item from the queue.
 * If the queue is empty it will return NULL.
 * @param queue the queue that you want to get an item from.
 */
void * readItem (Queue * const queue);

/**
 * Writes an item into the queue, if there is space.
 * If there is no space in the "write" sector it will try to recycle a sector
 * from "writeHead".
 *
 * In case of queue being full will return error with ENOMEM.
 * @param queue the queue to which to add a sector.
 * @param item the item that you want to add to the queue.
 * @return On success 0, -1 otherwise.
 */
int writeItem (Queue * const queue, void * const item);

/**
 * Submits a memory chunk that will become a 'QueueSector'.
 * The sector will be put at the head of the queue.
 *
 * If the memory chunk is to small you will get an error with ENOMEM.
 * @param queue the queue to which to add a sector.
 * @param meme the pointer to the chunk of the memory to be used.
 * @param size the size in bytes of the memory chunk.
 * @return On success 0, -1 otherwise.
 */
int submitSector(Queue * const queue, void * const mem, size_t const size);

/**
 * Tries to take out a sector if there is one empty.
 *
 * @param queue the queue from which to recover a sector.
 * @return the address of the sector, the address to the memory chunk that was
 * submitted.
 */
struct QueueSector * recoverSector(Queue * const queue);

#endif
