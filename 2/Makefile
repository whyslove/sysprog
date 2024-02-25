GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant -g

hw_2: parser.c solution.c 
	gcc $(GCC_FLAGS) parser.c solution.c -o hw_2

hw_2_with_leaks_check: parser.c solution.c ../utils/heap_help/heap_help.c
	gcc $(GCC_FLAGS) -ldl -rdynamic parser.c solution.c ../utils/heap_help/heap_help.c -o hw_2_with_leaks_check

clean:
	rm hw_2 hw_2_with_leaks_check