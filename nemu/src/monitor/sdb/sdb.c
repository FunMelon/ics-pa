#include "sdb.h"
#include "common.h"
#include "memory/paddr.h"
#include "utils.h"
#include <cpu/cpu.h>
#include <isa.h>
#include <readline/history.h>
#include <readline/readline.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin.
 */
static char *rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args) {
  uint n;
  sscanf(args, "%d", &n);
  cpu_exec(n);
  return 0;
}

static int cmd_info(char *args) {
  if (*args == 'r') {
    isa_reg_display();
  } else if (*args == 'w') {
    print_watchpoint();
  }
  return 0;
}

static int cmd_x(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg != NULL) {
    uint n;
    sscanf(arg, "%d", &n);
    bool success = true;
    word_t base = expr(arg + strlen(arg) + 1, &success);
    if (success) {
      printf("\033[32m");
      for (size_t i = 0; i < n; ++i)
        printf("%#010X\n", paddr_read(base + i * 4, 4));
      printf("\033[0m");
    } else {
      printf("\033[31mNot invaild expressions: %s\n\033[0m",
             arg + strlen(arg) + 1);
    }
  }
  return 0;
}

static int cmd_w(char *args) {
  if (args) {
    new_wp(args);
  }
  return 0;
}

static int cmd_d(char *args) {
  if (args) {
    int i = 0;
    sscanf(args, "%d", &i);
    free_wp(i);
  }
  return 0;
}

static int cmd_help(char *args);

static int cmd_p(char *args) {
  bool success = true;
  if (args) {
    word_t res = expr(args, &success);
    if (success) {
      printf("\033[32m%d\n\033[0m", res);
    } else {
      printf("\033[31mNot invaild expressions: %s\n\033[0m",
             args + strlen(args) + 1);
    }
  }
  return 0;
}

static struct {
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
    {"help", "Display informations about all supported commands", cmd_help},
    {"c", "Continue the execution of the program", cmd_c},
    {"q", "Exit NEMU", cmd_q},
    /* TODO: Add more commands */
    {"si", "Single step N instructions", cmd_si},
    {"info", "Print the information of the register", cmd_info},
    {"x", "Scan memory", cmd_x},
    {"w", "Set the watch point", cmd_w},
    {"d", "Delete the watch point", cmd_d},
    {"p", "Expression evaluation", cmd_p},

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i++) {
      printf("\033[32m%s - %s\n\033[0m", cmd_table[i].name,
             cmd_table[i].description);
    }
  } else {
    printf("\033[32m");
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name,
               cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
    printf("\033[0m");
  }
  return 0;
}

void sdb_set_batch_mode() { is_batch_mode = true; }

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL;) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) {
      continue;
    }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) {
          return;
        }
        break;
      }
    }

    if (i == NR_CMD) {
      printf("\033[31mUnknown command '%s'\n\033[0m", cmd);
    }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
