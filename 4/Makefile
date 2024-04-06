GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant -g

all: test.o thread_pool.o
	gcc $(GCC_FLAGS) test.o thread_pool.o

test.o: test.c thread_pool.h
	gcc $(GCC_FLAGS) -c test.c -o test.o -I ../utils

thread_pool.o: thread_pool.c thread_pool.h
	gcc $(GCC_FLAGS) -c thread_pool.c -o thread_pool.o

hw_4_with_leaks_check: test.c thread_pool.c ../utils/heap_help/heap_help.c
	gcc $(GCC_FLAGS) -g -ldl -rdynamic -I ../utils test.c thread_pool.c ../utils/heap_help/heap_help.c -o hw_4_with_leaks_check