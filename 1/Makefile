GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

hw_1: libcoro.c solution.c mergesort.c 
	gcc $(GCC_FLAGS) libcoro.c solution.c mergesort.c -o hw_1

hw_1_with_leaks_check: libcoro.c solution.c mergesort.c ../utils/heap_help/heap_help.c
	gcc $(GCC_FLAGS) -ldl -rdynamic libcoro.c solution.c mergesort.c ../utils/heap_help/heap_help.c -o hw_1_with_leaks_check

clean:
	rm a.out
