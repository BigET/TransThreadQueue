CPPFLAGS+=-DUSE_CORO_TEST
CFLAGS+=-O0 -ggdb

all: test
clean:
	rm -f *.o libcoro/*.o

test: test.o TransThread.o libcoro/coro.o

