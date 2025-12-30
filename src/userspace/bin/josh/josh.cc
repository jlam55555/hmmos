/// \file josh.c
/// \brief JOn's SHell.

// TODO: should we allow C++? or only support C?
//
// same with the header files: more relevant if we want to bootstrap
// other applications

#include "jlibc/ctype.h"
#include "jlibc/unistd.h"

#define MAX_CMD_LEN 4095
#define MAX_ARG_COUNT 1024

ssize_t read_cmd(char *buf, unsigned n) {
  // return read(stdin, buf, n);
  return 0;
}

void parse_cmd(char *buf, ssize_t n, char **args, int *argc) {
  *argc = 0;
  for (ssize_t i = 0; i < n; ++i) {
    if (isspace(buf[i])) {
      buf[i] = '\0';
    }
    if (i == 0 || buf[i - 1] == '\0' && !isspace(buf[i])) {
      args[(*argc)++] = &buf[i];
    }
  }
}

bool handle_builtin(char **args, int argc) { return false; }

void handle_cmd(char **args, int argc) {}

extern "C" {

void _start() {
  char cmd[MAX_CMD_LEN + 1];
  char *args[MAX_ARG_COUNT];
  int argc, n;

  while (1) {
    n = read_cmd(cmd, sizeof cmd);
    parse_cmd(cmd, n, args, &argc);
    if (argc == 0) {
      continue;
    }
    if (handle_builtin(args, argc)) {
      continue;
    }
    handle_cmd(args, argc);
  }

  __builtin_unreachable();
}
}
