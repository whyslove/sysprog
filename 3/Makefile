GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

all: test.o userfs.o
	gcc $(GCC_FLAGS) test.o userfs.o -o main

hw_3_with_leaks_check: test.c userfs.c ../utils/heap_help/heap_help.c
	gcc $(GCC_FLAGS) -g -ldl -rdynamic -I ../utils userfs.c test.c ../utils/heap_help/heap_help.c -o hw_3_with_leaks_check

test.o: test.c
	gcc $(GCC_FLAGS) -c test.c -o test.o -I ../utils

userfs.o: userfs.c
	gcc $(GCC_FLAGS) -c userfs.c -o userfs.o
