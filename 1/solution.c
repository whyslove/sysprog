#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libcoro.h"
#include "mergesort.h"

struct IntArray {
  int *data;
  int length;
};

int read_integers_from_file(const char *filename, struct IntArray *result) {
  FILE *file = fopen(filename, "r");  // Open the file for reading
  if (file == NULL) {
    perror("Error opening the file");
    return 1;
  }

  int capacity = 10;
  int size = 0;
  int *integers = (int *)malloc(capacity * sizeof(int));

  if (integers == NULL) {
    fclose(file);
    return 1;
  }

  int number;
  while (fscanf(file, "%d", &number) == 1) {
    integers[size++] = number;

    if (size >= capacity) {
      capacity *= 2;
      int *temp = realloc(integers, capacity * sizeof(int));
      if (temp == NULL) {
        free(integers);
        fclose(file);
        return 1;
      }
      integers = temp;
    }
  }

  if (ferror(file) != 0) {
    fclose(file);
    free(integers);
    return 1;
  }

  result->data = integers;
  result->length = size;

  fclose(file);
  return 0;
}

int write_integers_to_file(int *array, int len) {
  FILE *file = fopen("result.txt", "w");
  if (file == NULL) {
    printf("Error opening the file");
    return 1;
  }

  for (int i = 0; i < len; i++) {
    fprintf(file, "%d ", array[i]);
  }

  fclose(file);

  return 0;
}

struct my_context {
  char *filename;
  struct IntArray *array;
  /** ADD HERE YOUR OWN MEMBERS, SUCH AS FILE NAME, WORK TIME, ... */
};

static struct my_context *
my_context_new(const char *name, struct IntArray *array) {
  struct my_context *ctx = malloc(sizeof(*ctx));
  ctx->filename = strdup(name);
  ctx->array = array;
  return ctx;
}

static void
my_context_delete(struct my_context *ctx) {
  free(ctx->filename);
  free(ctx);
}

/**
 * A function, called from inside of coroutines recursively. Just to demonstrate
 * the example. You can split your code into multiple functions, that usually
 * helps to keep the individual code blocks simple.
 */
// static void
// other_function(const char *name, int depth) {
//   printf("%s: entered function, depth = %d\n", name, depth);
//   coro_yield();
//   if (depth < 3)
//     other_function(name, depth + 1);
// }

int int_gt_comparator(const void *a, const void *b) {
  return *(int *)a - *(int *)b;
}
/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */
static int
coroutine_func_f(void *context) {
  //   struct coro *this = coro_this();
  struct my_context *ctx = context;

  if (read_integers_from_file(ctx->filename, ctx->array) != 0) {
    printf("Error reading integers from file %s", ctx->filename);
    return 1;
  }
  yield_if_period_end();

  mergesort(ctx->array->data, ctx->array->length, sizeof(int), int_gt_comparator);

  my_context_delete(ctx);
  /* This will be returned from coro_status(). */
  return 0;
}

int main(int argc, char **argv) {
  /* Initialize our coroutine global cooperative scheduler. */
  coro_sched_init();

  int num_of_files = argc - 2;
  int files_offset = 2;
  int msec_time_slice = atoi(argv[1]) / num_of_files;

  /* Initialize memory for arrays and start several coroutines which will process memory */
  struct IntArray **arrays = malloc(sizeof(struct IntArray) * (argc - 1));
  for (int i = 0; i < num_of_files; ++i) {
    struct IntArray *array = malloc(sizeof(struct IntArray));
    coro_new(coroutine_func_f, my_context_new(argv[i + files_offset], array), msec_time_slice);
    arrays[i] = array;
  }

  /* Wait for all the coroutines to end. */
  struct coro *c;
  while ((c = coro_sched_wait()) != NULL) {
    coro_delete(c);
  }

  int *res_arr = malloc(0);
  int res_len = 0;
  for (int i = 0; i < num_of_files; ++i) {
    int *temp_dst = malloc((res_len + arrays[i]->length) * sizeof(int));

    merge(res_arr, arrays[i]->data, res_len, arrays[i]->length, sizeof(int), int_gt_comparator, temp_dst);
    free(arrays[i]->data);
    res_len += arrays[i]->length;
    free(arrays[i]);
    free(res_arr);
    res_arr = temp_dst;
  }

  if (write_integers_to_file(res_arr, res_len) != 0) {
    printf("Error writing to file");
    return 1;
  }

  free(res_arr);
  free(arrays);

  return 0;
}
