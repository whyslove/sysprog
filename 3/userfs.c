#include "userfs.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "stdio.h"

enum {
  BLOCK_SIZE = 512,
  MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
  /** Block memory. */
  char *memory;
  /** How many bytes are occupied. */
  int occupied;
  /** Next block in the file. */
  struct block *next;
  /** Previous block in the file. */
  struct block *prev;

  /* PUT HERE OTHER MEMBERS */
};

struct file {
  /** Double-linked list of file blocks. */
  struct block *block_list;
  /**
   * Last block in the list above for fast access to the end
   * of file.
   */
  struct block *last_block;
  /** How many file descriptors are opened on the file. */
  int refs;
  /** File name. */
  char *name;
  /** Files are stored in a double-linked list. */
  struct file *next;
  struct file *prev;

  int is_deleted;

  /* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
  struct file *file;

  int pos;

  /* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno() {
  return ufs_error_code;
}

int get_free_fd_adress() {
  if (file_descriptor_capacity == 0) {
    file_descriptor_capacity = 1;
    file_descriptors = malloc(sizeof(struct filedesc *));
    file_descriptors[0] = NULL;
  }

  if (file_descriptor_count == file_descriptor_capacity) {
    file_descriptors = realloc(file_descriptors, sizeof(struct filedesc *) * file_descriptor_capacity * 2);
    if (file_descriptors == NULL) {
      ufs_error_code = UFS_ERR_NO_MEM;
      return -1;
    }
    // for (int i = file_descriptor_capacity; i < file_descriptor_capacity * 2; ++i)
    //   file_descriptors[i] = NULL;
    // printf("BEFORE MEMSET %ld %ld %ld %d\n", (long)file_descriptors, (long)(file_descriptors + sizeof(struct filedesc *) * file_descriptor_capacity), sizeof(struct filedesc *), file_descriptor_capacity);
    memset(file_descriptors + file_descriptor_capacity, 0, sizeof(struct filedesc *) * file_descriptor_capacity);
    file_descriptor_capacity = file_descriptor_capacity * 2;
  }

  for (int i = 0; i < file_descriptor_capacity; i++) {
    if (file_descriptors[i] == NULL) {
      file_descriptor_count++;
      return i;
    }
  }

  return -1;
}

struct file *file_find(const char *filename) {
  struct file *file = file_list;
  while (file != NULL) {
    if (!strcmp(file->name, filename)) {
      return file;
    }

    // printf("INVALID READ?\n");
    file = file->prev;
    // printf("INVALID READ!\n");
  }

  return NULL;
}

struct file *file_create(const char *filename) {
  struct file *file = malloc(sizeof(struct file));
  if (file == NULL) {
    ufs_error_code = UFS_ERR_NO_MEM;
    return NULL;
  }

  file->block_list = NULL;
  file->last_block = NULL;
  file->name = strdup(filename);
  file->next = NULL;
  file->prev = NULL;
  file->refs = 0;
  file->is_deleted = 0;

  if (file_list == NULL) {
    // printf("CREATED FILE IN file_create AT ADRESS %ld \n", (long)file);
    file_list = file;
    return file;
  }

  file->next = file_list;
  file->prev = file_list->prev;

  if (file_list->prev != NULL)
    file_list->prev->next = file;
  file_list->prev = file;

  // printf("CREATED FILE IN file_create AT ADRESS %ld \n", (long)file);
  return file;
}

int ufs_open(const char *filename, int flags) {
  int fd = get_free_fd_adress();
  if (fd == -1) {
    ufs_error_code = USF_ERR_INTERNAL;
    return -1;
  }

  struct file *file = file_find(filename);
  if (file == NULL || file->is_deleted) {
    if (flags & UFS_CREATE) {
      file = file_create(filename);
      if (file == NULL) {
        return -1;
      }
    } else {
      ufs_error_code = UFS_ERR_NO_FILE;
      return -1;
    }
  }

  file_descriptors[fd] = malloc(sizeof(struct filedesc));
  if (file_descriptors[fd] == NULL) {
    ufs_error_code = UFS_ERR_NO_MEM;
    return -1;
  }
  // printf("RECEIVING FD AT ADRESS %ld, FD: %d\n", (long)file_descriptors[fd], fd);

  file_descriptors[fd]->pos = 0;

  file->refs++;
  file_descriptors[fd]->file = file;

  return fd;
}

int min(int a, int b) {
  return a < b ? a : b;
}

int max(int a, int b) {
  return a > b ? a : b;
}

int file_write(struct file *file, const char *buf, size_t size, int pos) {
  int start_pos = pos;
  struct block *block = file->block_list;
  struct block *block_prev = NULL;

  int block_num = 0;
  while (pos / (BLOCK_SIZE * (block_num + 1)) > 0) {
    block_prev = block;
    block = block->next;
    block_num++;
  }

  int start_block_offset = (pos - BLOCK_SIZE * (block_num)) % BLOCK_SIZE;

  while ((size_t)pos < start_pos + size) {
    if (block == NULL) {
      block = malloc(sizeof(struct block));  // TODO: в задании сказали, можно не обрабатывать ошибки
      block->memory = malloc(BLOCK_SIZE);
      block->occupied = 0;
      block->next = NULL;
      if (block_prev != NULL) {
        block->prev = block_prev;
        block->prev->next = block;
      } else {
        file->block_list = block;
      }
    }

    int to_copy = min(BLOCK_SIZE - start_block_offset, size - (pos - start_pos));
    // printf("%ld, %ld, %d\n", (long)block->memory, (long)(block->memory + start_block_offset), to_copy);
    memcpy(block->memory + start_block_offset, buf + (pos - start_pos), to_copy);
    block->occupied = max(start_block_offset + to_copy, block->occupied);
    pos += to_copy;

    block_prev = block;
    block = block->next;
    start_block_offset = 0;
  }

  return size;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size) {
  if (fd < 0 || fd > file_descriptor_capacity || file_descriptors[fd] == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }
  struct file *file = file_descriptors[fd]->file;

  if (file_descriptors[fd]->pos + size > MAX_FILE_SIZE) {
    ufs_error_code = UFS_ERR_NO_MEM;
    return -1;
  }
  int n = file_write(file, buf, size, file_descriptors[fd]->pos);
  file_descriptors[fd]->pos += n;

  return size;
}

int file_read(struct file *file, char *buf, size_t size, int pos) {
  int start_pos = pos;
  struct block *block = file->block_list;

  int block_num = 0;
  int start_block_offset = 0;
  while (pos / (BLOCK_SIZE * (block_num + 1)) > 0) {
    block = block->next;
    block_num++;
  }

  start_block_offset = (pos - BLOCK_SIZE * (block_num)) % BLOCK_SIZE;
  //   printf("off occ %d, %d\n", start_block_offset, block->occupied);

  while ((size_t)pos < pos + size && block != NULL) {
    int to_copy = min(block->occupied - start_block_offset, size - (pos - start_pos));
    // printf("to_copy: %d iii %d\n", to_copy, block->occupied - start_block_offset);
    memcpy(buf + (pos - start_pos), block->memory + start_block_offset, to_copy);
    // printf("to_copy: %d iii %d\n", to_copy, block->occupied - start_block_offset);
    pos += to_copy;

    block = block->next;
    start_block_offset = 0;
  }

  //   printf("%d - %d, %ld \n", pos, start_pos, size);
  return pos - start_pos;
}

ssize_t
ufs_read(int fd, char *buf, size_t size) {
  if (fd < 0 || fd > file_descriptor_capacity || file_descriptors[fd] == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }
  struct file *file = file_descriptors[fd]->file;

  int n = file_read(file, buf, size, file_descriptors[fd]->pos);
  file_descriptors[fd]->pos += n;

  return n;
}

void file_delete(struct file *file) {
  if (file->refs > 0) {
    return;
  }

  struct block *block = file->block_list;
  while (block != NULL) {
    // printf("BLOCK FREE MEMORY FILE IN file_delete AT ADRESS %ld \n", (long)block->memory);
    free(block->memory);
    struct block *next = block->next;
    // printf("BLOCK FREE BLOCK FILE IN file_delete AT ADRESS %ld \n", (long)block);
    free(block);
    block = next;
  }

  if (file->prev == NULL && file->next == NULL) {
    file_list = NULL;
  } else if (file->prev == NULL && file->next != NULL) {
    file->next->prev = NULL;
    // pass
  } else if (file->prev != NULL && file->next == NULL) {
    file->prev->next = NULL;
    file_list = file->prev;
  } else {
    file->prev->next = file->next;
    file->next->prev = file->prev;
  }

  // printf("FREE FILE IN file_delete AT ADRESS %ld \n", (long)file);
  free(file->name);
  free(file);
}

int ufs_close(int fd) {
  if (fd < 0 || fd > file_descriptor_capacity || file_descriptors[fd] == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  // printf("CLOSING FD AT ADRESS %ld %d\n", (long)file_descriptors[fd], fd);
  struct file *file = file_descriptors[fd]->file;
  file->refs--;
  if (file->refs == 0 && file->is_deleted != 0) {
    // printf("DELETING FILE IN ufs_close AT ADRESS %ld \n", (long)file);
    file_delete(file);
  }

  // printf("FREE FILE_DESCRIPTOR %ld \n", (long)file_descriptors[fd]);
  free(file_descriptors[fd]);
  file_descriptors[fd] = NULL;
  file_descriptor_count--;
  return 0;
}

int ufs_delete(const char *filename) {
  struct file *file = file_find(filename);
  if (file == NULL) {
    return 0;
  }

  file->is_deleted = 1;

  // printf("DELETING FILE IN ufs_delete AT ADRESS %ld \n", (long)file);
  file_delete(file);
  return 0;
}

void ufs_destroy(void) {
  struct file *file = file_list;
  while (file != NULL) {
    struct file *to_delete = file;
    file = file->prev;
    file_delete(to_delete);
  }

  for (int i = 0; i < file_descriptor_capacity; i++) {
    if (file_descriptors[i] != NULL) {
      free(file_descriptors[i]);
    }
  }
  free(file_descriptors);
}
