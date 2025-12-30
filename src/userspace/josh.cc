/// \file josh.c
/// \brief JOn's SHell.

#define MAX_CMD_LEN 4095
#define MAX_ARG_COUNT 1024

void read_cmd(char *buf, unsigned n) {}

void parse_cmd(char *buf, char **args, int *argc) {}

bool handle_builtin(char **args, int argc) { return false; }

void handle_cmd(char **args, int argc) {}

void _start() {
  char cmd[MAX_CMD_LEN + 1];
  char *args[MAX_ARG_COUNT];
  int argc;

  while (1) {
    read_cmd(cmd, sizeof cmd);
    parse_cmd(cmd, args, &argc);
    if (argc == 0) {
      continue;
    }
    if (handle_builtin(args, argc)) {
      continue;
    }
    handle_cmd(args, argc);
  }
}
