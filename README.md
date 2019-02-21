# TransThreadQueue
A queue that passes info from one thread to another without locks.

# A little theory.

What are the axioms that this inter thread communication relies?

A value of a pointer or an integer is written or read atomically.

This opens some pretty interesting consistency techniques.

We use "volatile" to tell the compile to not optimize the access to the memory
locations that are targeted by more than one thread.

Also we are relying on the cache coherency implemented in the CPUs to make
sure that if we are touching a location, any other CPU will get the new value
when it is trying to read it.

With that being said, let's see what we need to do to make sure that a variable
is thread safe.

From here on we will accept that there are only 2 threads.

* The writer thread is the thread that writes items in the queue and also
allocates and deallocates sectors in it.

* The reader thread is the thread that reads items from the queue.

## A constant variable. Written only once in its lifetime.

A variable that is written only once, when the structure is initialized,
before the structure is visible to the other thread, is thread safe.

One example is QueueSector.size that is written by the once by the write
thread when it is initializing the structure just before adding it to the queue.

## A simple variable. Writen by only one thread.

A variable that is written only by a single thread in the whole life of the
variable is thread safe.

One example is Queue.activeRead that is written only by the read thread.

## A guarded variable. Written by both thread but in disjunct scenarios.

Let's look at the QueueSector.readCursor.

This variable is used by the read thread usually, but the read thread is
mutating it only if readCursor < writeCursor.

If readCursor >= writeCursor then the read thread will not mutate anything in
that QueueSector.

So when readCursor >= writeCursor the write thread is free to mutate anything
in that QueueSector providing that at any time the above condition still holds.
As the last step the write thread may do something that make the condition false
and then it can no longer touch readCursor until the condition is true again.

## An "X" guard. The most complicated access pattern.

Let's look at Queue.read and Queue.activeRead.

When Queue.read is 0 the read thread will just pops in
(puts Queue.activeRead on 1) see that Queue.read is 0 and pops out
(puts Queue.activeRead on 0).

Only the write thread can mutate Queue.read when is 0 to something that is not 0.
This happens when submitting a sector and the queue is empty of sectors.

When the write wants to recover the last sector and the queue itself is empty.
It sets Queue.read to 0. This does not means that there is not a read thread
that is already accessing that last sector, so only if Queue.activeRead is 0
it will free the last sector, otherwise it will fail to free and the client will
have to try again.

Because the resources are mutated and access in different orders in the read
and write thread (that is why it is an "X" guard).
It ensures that the write thread can free the last sector only if there is no
read thread active and any read that will comes will get the 0 Queue.read so
it will no longer reach the sector.

# How the theory works on QueueSector?

The readCursor is always chasing the writeCursor.

Because we don't have locks the user have to cope with a writeItem failing
because there is no space left and a readItem failing because the queue is
empty.

## QueueSector.size

It is populated on initialization by the write thread before the sector is added
to the queue, after that is constant.

## QueueSector.readCursor

When QueueSector.readCursor < QueueSector.writeCursor the variable is used only
by the read thread and it is mutated only in one way (increment only).
So once QueueSector.readCursor == QueueSector.writeCursor the read thread will
not mutate anything so the write thread can mess around with the sector.

## QueueSector.writeCursor

Mutated only by the write thread.

## QueueSector.nextSector

Mutated only by the write thread.

## QueueSector.items

An array of items (pointer to items rather).

* [0, readCursor) -> undefined (old) data.

* [readCursor, writeCursor) -> recorded data that will be read.

* [writeCursor, size) -> undefined (yet to be written) data.

# How the theory work on Queue?

Here we also have 3 pointers chasing each other.

We have a single linked list of sectors starting from Queue.writeHead and
finishing with Queue.write. So an invariant is that, if Queue.write is not 0,
write->nextSector is 0.

Queue.read is pointing to one of the sectors of this list. All the sectors from
Queue.writeHead up to, but not including, Queue.read are spare sectors.
All the sectors from Queue.read up to including Queue.write are sectors with
live data.

When the Queue.write sector is full a writeItem will move the first spare
sector to the end of the queue and link it there.

The allocation and deallocation of sectors is done on the write thread.
The allocation will fail only if you give a memory chunk that is to small
(less then 3 ints and 2 pointers), but the deallocation can fail if there is
no spare sector to reclaim.

## Queue.writeHead

Mutated only by the write thread.

## Queue.write

Mutated only by the write thread.

## Queue.read

Is mutated by both threads and we have discussed above in which way.

## Queue.activeRead

Mutated only by the read thread.

