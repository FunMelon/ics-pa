#ifndef __SDB_H__
#define __SDB_H__

#include <common.h>

word_t expr(char *e, bool *success);

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  struct watchpoint *pre;
  /* TODO: Add more members if necessary */
  char *expr; // 表达式
  bool used;
} WP;

void new_wp(char *expr);

void free_wp(int no);

void print_watchpoint();

bool check_points();

#endif
