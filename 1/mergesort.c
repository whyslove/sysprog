#include "mergesort.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "libcoro.h"

void my_memcpy(void *_dst, void *_src, size_t n) {
  char *dst = (char *)_dst;
  char *src = (char *)_src;
  while (n) {
    *(dst++) = *(src++);
    n--;
  }
}

void merge(
    void *left_start, void *right_start,
    size_t left_size, size_t right_size,
    size_t element_size,
    int (*comparator)(const void *, const void *),
    void *result) {
  size_t cur_left = 0, cur_right = 0, cur_result = 0;
  while (cur_left < left_size && cur_right < right_size) {
    if (comparator(left_start + cur_left * element_size, right_start + cur_right * element_size) <= 0) {
      my_memcpy(result + cur_result * element_size, left_start + cur_left * element_size, element_size);
      cur_left++;
    } else {
      my_memcpy(result + cur_result * element_size, right_start + cur_right * element_size, element_size);
      cur_right++;
    }
    cur_result++;
  }

  while (cur_left < left_size) {
    my_memcpy(result + cur_result * element_size, left_start + cur_left * element_size, element_size);
    cur_left++;
    cur_result++;
  }

  while (cur_right < right_size) {
    my_memcpy(result + cur_result * element_size, right_start + cur_right * element_size, element_size);
    cur_right++;
    cur_result++;
  }
}

int mergesort(
    void *array,
    size_t elements,
    size_t element_size,
    int (*comparator)(const void *, const void *)) {
  if (elements <= 1) {
    return 0;
  }

  size_t middle = elements / 2;
  void *left = array;
  void *right = (char *)array + middle * element_size;

  int mergesort_res;
  mergesort_res = mergesort(left, middle, element_size, comparator);
  if (mergesort_res == -1) {
    return -1;
  }
  yield_if_period_end();

  mergesort(right, elements - middle, element_size, comparator);
  if (mergesort_res == -1) {
    return -1;
  }
  yield_if_period_end();

  void *temp = malloc(elements * element_size);
  if (!temp) {
    return -1;
  }
  merge(left, right, middle, elements - middle, element_size, comparator, temp);
  my_memcpy(array, temp, elements * element_size);
  free(temp);

  return 0;
}
