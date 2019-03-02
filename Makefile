all: test

test: test.o TransThread.o libcoro/coro.o

