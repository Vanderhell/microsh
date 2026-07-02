CC ?= gcc
CXX ?= c++
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -Werror -I../include
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wpedantic -Werror -I../include
SRC := ../src/msh.c
UNIT := test_all.c

.PHONY: all clean consumer-c consumer-cpp compile-fail

all: test_all
	@./test_all

test_all: $(UNIT) $(SRC) ../include/msh.h
	$(CC) $(CFLAGS) $(SRC) $(UNIT) -o $@

consumer-c: consumers/c_consumer.c consumers/abi_guard_ref.c $(SRC) ../include/msh.h
	$(CC) $(CFLAGS) $(SRC) consumers/c_consumer.c consumers/abi_guard_ref.c -o consumer_c

consumer-cpp: consumers/cpp_consumer.cpp consumers/abi_guard_ref.c $(SRC) ../include/msh.h
	$(CC) $(CFLAGS) -c $(SRC) -o msh.o
	$(CC) $(CFLAGS) -c consumers/abi_guard_ref.c -o abi_guard_ref.o
	$(CXX) $(CXXFLAGS) consumers/cpp_consumer.cpp msh.o abi_guard_ref.o -o consumer_cpp

compile-fail:
	@echo "Compile-fail sources live in tests/compile_fail/"

clean:
	rm -f test_all consumer_c consumer_cpp msh.o abi_guard_ref.o
