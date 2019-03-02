all: test
clean:
	rm -f *.o libcoro/*.o

test: test.o TransThread.o libcoro/coro.o

