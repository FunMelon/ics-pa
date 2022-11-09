#include "common.h"
#include "sdb.h"
#include <string.h>

#define NR_WP 32

// typedef struct watchpoint {
//   int NO;
//   struct watchpoint *next;
//
//   /* TODO: Add more members if necessary */
//   char *expr;  // 表达式
// } WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i++) {
    wp_pool[i].NO = i;
    wp_pool[i].used = false; // 设置初始状态未被占用
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    wp_pool[i].pre = (i == 0 ? NULL : &wp_pool[i - 1]);
  }

  head = NULL;
  free_ = wp_pool;
  wp_pool[0].pre = free_; // 增加了该语句作为前置
}

/* TODO: Implement the functionality of watchpoint */

// 从free_链表中返回空闲监视点结构，采用从顶部开始“削”的方式
void new_wp(char *expr) {
  if (!free_)
    panic("\033[31m没有足够的空闲监视点供分配\n\033[0m");
  WP *ptr = free_;
  if (free_->next != NULL)
    free_->next->pre = NULL; // 头节点的前置指针为空
  free_ = free_->next;
  // 分配新的空间
  ptr->expr = malloc((strlen(expr) + 1) * sizeof(char));
  // printf("长度是: %d", (int)strlen(expr));
  strcpy(ptr->expr, expr);
  ptr->used = true;
  // 加入
  assert(ptr);
  if (head)
    head->pre = ptr;
  ptr->next = head;
  head = ptr;
  printf("\033[33m成功设置监视点，[No: %5d]: %s\n\033[0m", ptr->NO, ptr->expr);
}

// 将wp归还到free_链表中
void free_wp(int no) {
  if (no < 0 || no > NR_WP) {
    printf("\033[31m%d 不在监视点序号的范围内！\n\033[0m", no);
    return;
  } else if (wp_pool[no].used == false) {
    printf("\033[31m%d 未设置该监视点！\n\033[0m", no);
    return;
  }
  // 进行链表的再组合
  // 1.拆
  if (head == &wp_pool[no]) // 切换指针
    head = wp_pool[no].next;
  if (wp_pool[no].next != NULL) // 在原表中不是尾节点
    wp_pool[no].next->pre = wp_pool[no].pre;
  if (wp_pool[no].pre != NULL) // 在原表中不是头节点
    wp_pool[no].pre->next = wp_pool[no].next;
  // 2.入
  if (free_) // free_不为空表的时候
    free_->pre = &wp_pool[no];
  wp_pool[no].next = free_;
  free_ = &wp_pool[no]; // 回收内存
  free(wp_pool[no].expr);
  wp_pool[no].used = false;
  wp_pool[no].expr = NULL;
  printf("\033[33m成功删除监视点，[No: %5d]: %s\n\033[0m", wp_pool[no].NO, wp_pool[no].expr);
}

// 打印所有的监视点信息
void print_watchpoint() {
  printf("\033[32m");
  for (WP *ptr = head; ptr != NULL; ptr = ptr->next) {
    printf("[No: %5d]: %s\n", ptr->NO, ptr->expr);
  }
  printf("\033[0m");
}

// 检查断点信息
bool check_points() {
  bool isTriggered = false;
  bool success;
  for (WP *ptr = head; ptr != NULL; ptr = ptr->next) {
    word_t res = expr(ptr->expr, &success);
    if (res == 1){
      printf("\033[31m[No: %5d]: %s was triggered\n \033[0m", ptr->NO, ptr->expr);
      isTriggered = true;
    }
  }
  return isTriggered;
}
