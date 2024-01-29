#pragma once
#include <stddef.h>

int mergesort(
    void *array,
    size_t elements, size_t element_size,
    int (*comparator)(const void *, const void *));

void my_memcpy(void *_dst, void *_src, size_t n);

void merge(
    void *left_start, void *right_start,
    size_t left_size, size_t right_size,
    size_t element_size,
    int (*comparator)(const void *, const void *),
    void *result);
