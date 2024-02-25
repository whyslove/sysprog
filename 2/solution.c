#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "parser.h"

int execute_cd(const struct expr *e) {
  if (e->cmd.arg_count == 0) {
    return 0;
  }

  return chdir(e->cmd.args[0]);
}

void execute_exit(const struct expr *e) {
  if (e->cmd.arg_count == 0) {
    exit(0);
  }

  exit(atoi(e->cmd.args[0]));
}

void execute_base_command(const struct expr *e) {
  if (!strcmp(e->cmd.exe, "exit")) {
    execute_exit(e);
    return;
  }

  char *args_for_execvp[e->cmd.arg_count + 2];
  args_for_execvp[0] = e->cmd.exe;
  for (uint32_t i = 0; i < e->cmd.arg_count; ++i) {
    args_for_execvp[i + 1] = e->cmd.args[i];
  }
  args_for_execvp[e->cmd.arg_count + 1] = NULL;

  execvp(e->cmd.exe, args_for_execvp);
}

static int execute_command_line(const struct command_line *line) {
  int last_status = 0;

  int last_pid;
  int pipe_fd[2];
  int use_pipe_as_stdin = 0;

  int proc_to_wait = 1;
  const struct expr *e = line->head;
  while (e != NULL) {
    if (e->type == EXPR_TYPE_COMMAND) {
      if (e->next && e->next->type == EXPR_TYPE_PIPE) {
        pipe(pipe_fd);
      }

      if (!strcmp(e->cmd.exe, "exit") && !e->next && line->head == e) {
        if (e->cmd.arg_count == 0) {
          // return 0;
          exit(0);
          // last_status = 0;
        } else {
          // return atoi(e->cmd.args[0]);
          exit(atoi(e->cmd.args[0]));
        }

      } else if (!strcmp(e->cmd.exe, "cd")) {
        execute_cd(e);
      } else {
        last_pid = fork();
        // Любые другие команды
        if (last_pid == 0) {
          // Тюним а точно ли читаем из STDIN
          if (use_pipe_as_stdin != 0) {
            dup2(use_pipe_as_stdin, STDIN_FILENO);
            close(use_pipe_as_stdin);
            use_pipe_as_stdin = 0;
          }

          // Тюним куда хотим выводить
          int fd = STDOUT_FILENO;
          if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
            fd = open(line->out_file, O_APPEND | O_RDWR | O_APPEND, 0664);
          }
          if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
            fd = open(line->out_file, O_CREAT | O_WRONLY | O_TRUNC, 0664);
          }
          if (e->next && e->next->type == EXPR_TYPE_PIPE) {
            close(pipe_fd[0]);
            fd = pipe_fd[1];
          }

          if (fd != STDOUT_FILENO) {
            dup2(fd, STDOUT_FILENO);
            close(fd);
          }

          execute_base_command(e);
        }

        if (use_pipe_as_stdin) {
          close(use_pipe_as_stdin);
        }

        if (e->next && e->next->type == EXPR_TYPE_PIPE) {
          use_pipe_as_stdin = pipe_fd[0];  // Пока еще не prev, но в этом процессе он уже не используется
          close(pipe_fd[1]);
        }

        proc_to_wait++;
      }
    } else if (e->type == EXPR_TYPE_PIPE) {
      // printf("\tPIPE\n");
    } else if (e->type == EXPR_TYPE_AND) {
      // printf("\tAND\n");
    } else if (e->type == EXPR_TYPE_OR) {
      // printf("\tOR\n");
    } else {
      assert(false);
    }
    e = e->next;
  }

  for (int i = 0; i < proc_to_wait; ++i) {
    int stat;
    int waited_pid = wait(&stat);
    if (waited_pid == last_pid) {
      last_status = WEXITSTATUS(stat);
    }
  }

  return last_status;
}

int main(void) {
  int last_status = 0;
  const size_t buf_size = 1024;
  char buf[buf_size];
  int rc;
  struct parser *p = parser_new();
  while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
    parser_feed(p, buf, rc);
    struct command_line *line = NULL;
    while (true) {
      enum parser_error err = parser_pop_next(p, &line);
      if (err == PARSER_ERR_NONE && line == NULL)
        break;
      if (err != PARSER_ERR_NONE) {
        printf("Error: %d\n", (int)err);
        continue;
      }
      last_status = execute_command_line(line);
      command_line_delete(line);
    }
  }
  parser_delete(p);
  return last_status;
}
