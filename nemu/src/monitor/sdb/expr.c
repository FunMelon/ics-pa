#include "debug.h"
#include "memory/paddr.h"
#include <assert.h>
#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <limits.h>
#include <regex.h>

#define NULL_NUM 0x80000000
#define priority(op) (op.type / 10) // 获取优先级

enum {
  /* TODO: Add more token types */
  TK_NOTYPE = 0,
  // 可以比较的运算符
  TK_PLUS = 10,  // 加
  TK_SUB = 11,   // 减
  TK_ASTER = 20, // 乘，取地址
  TK_DIV = 21,   // 除

  TK_EQ = 30, // 监视点
  TK_LT = 31, // 小于号
  TK_MT = 32, // 大于号

  TK_HEX = 40, // 十六进制
  TK_DEC = 41, // 十进制
  TK_REG = 42, // 寄存器

  // 不可比运算符
  TK_LBT = 61, // 左括号
  TK_RBT = 62, // 右括号

  // 暂时保留
  TK_AND, // 与
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */

    {" +", TK_NOTYPE},               // spaces
    {"\\+", TK_PLUS},                // plus
    {"==", TK_EQ},                   // equal
    {"<", TK_LT},                    // less than
    {">", TK_MT},                    // more than
    {"&&", TK_AND},                  // and
    {"-", TK_SUB},                   // 减号
    {"\\*", TK_ASTER},               // 乘号或解引用号
    {"/", TK_DIV},                   // 除号
    {"\\(", TK_LBT},                 // 左括号
    {"\\)", TK_RBT},                 // 右括号
    {"0[x|X][A-Fa-f0-9]+", TK_HEX},  // 十六进制数
    {"0|[1-9][0-9]*", TK_DEC},       // 十进制数
    {"\\$[a-z0-9\\$]{2,3}", TK_REG}, // 寄存器
};

#define NR_REGEX ARRLEN(rules)
static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i,
        //     rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        if (rules[i].token_type != TK_NOTYPE) { // 不为空，加入符号表
          tokens[nr_token].type = rules[i].token_type;
          switch (rules[i].token_type) { // 特殊的数组存储额外信息
          case TK_DEC:
          case TK_HEX:
          case TK_REG:
            if (substr_len >
                sizeof(tokens->str) / sizeof(char)) // 防止缓冲区溢出
              return false;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            break;
            //   TODO();
          }
          ++nr_token;                                     // 追踪数组末尾
          if (nr_token == sizeof(tokens) / sizeof(Token)) // 防止缓冲区溢出
            return false;
        }
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

// 检查表达式是否在整括号内
// 1.括号要匹配，2.最外层的括号要匹配
// 返回值为0，不合法的括号，1不符合BNF的括号，2符号BNF的括号
static int check_parentheses(size_t p, size_t q) {
  // 检查是否含有最外层括号
  bool isLegal = true;
  bool isBNF = true;
  if (!(tokens[p].type == TK_LBT && tokens[q].type == TK_RBT))
    return false;
  // 检查括号是否匹配
  int n = 0;
  while (p < q) {
    if (tokens[p].type == TK_LBT)
      ++n;
    else if (tokens[p].type == TK_RBT)
      --n;

    if (n < 0)
      isLegal = false;
    else if (n == 0)
      isBNF = false;
    ++p;
  }
  // Log("%d %d", isBNF, isLegal);
  return isBNF ? 2 : (isLegal ? 1 : 0);
}

// 寻找主运算符
static size_t findMainOp(size_t p, size_t q) {
  size_t op = p;

  bool isLastOp = true; // 标注上一个符号是否是运算符
  while (p < q) {
    if (tokens[p].type ==
        TK_LBT) { // 遇见左括号则进入步进模式，直到移动到右括号为止
      while (tokens[p].type != TK_RBT)
        ++p;
      isLastOp = false;
      ++p;
    } else {
      // 最直接的优先级判断
      if ((tokens[p].type == TK_SUB || tokens[p].type == TK_ASTER) &&
          isLastOp) { // 略过单目运算符，只在递归最底层计算
        ++p;
        continue;
      } else if (priority(tokens[p]) <= priority(tokens[op])) {
        op = p;
      }
      // 根据本次运算符的性质为下一次判断做辅助
      if (tokens[p].type != TK_HEX && tokens[p].type != TK_DEC &&
          tokens[p].type != TK_REG) { // 是运算符不是数字
        isLastOp = true;
      } else {
        isLastOp = false;
      }
      ++p;
    }
  }
  // Log("找到的主运算符为: %d", (int)op);
  // assert(tokens[op].type == TK_PLUS || tokens[op].type == TK_SUB ||
  //        tokens[op].type == TK_MT || tokens[op].type == TK_LT ||
  //        tokens[op].type == TK_EQ || tokens[op].type == TK_ASTER ||
  //        tokens[op].type == TK_DIV);
  return op;
}

// 返回数字
word_t toNum(size_t p, bool *success) {
  assert(tokens[p].type == TK_REG || tokens[p].type == TK_DEC ||
         tokens[p].type == TK_HEX);

  word_t num = 0;
  if (tokens[p].type == TK_DEC) {
    sscanf(tokens[p].str, "%d", &num);
  } else if (tokens[p].type == TK_HEX) {
    sscanf(tokens[p].str, "%x", &num);
  } else if (tokens[p].type == TK_REG) {
    num = isa_reg_str2val(tokens[p].str + 1, success);
  }
  // Log("返回的数字是: %d", num);
  return num;
}

// 计算表达式
static word_t eval(size_t p, size_t q, bool *success) {
  if (p > q) { // 单目运算符才会出现的情况
    return NULL_NUM;
  } else if (p == q) {
    return toNum(p, success);
  } else if (check_parentheses(p, q) == 2) { // BNF
    return eval(p + 1, q - 1, success);
  } else { // 也许合法但不匹配
    size_t op = findMainOp(p, q);
    word_t val1 = eval(p, op - 1, success);
    word_t val2 = eval(op + 1, q, success);

    switch (tokens[op].type) {
    case TK_PLUS:
      return val1 + val2;
    case TK_SUB:
      if (val1 == NULL_NUM)
        return -val2;
      else
        return val1 - val2;
    case TK_ASTER:
      if (val1 == NULL_NUM) { // 取地址符，默认取32位
        // Log("待取地址的基址为: %d", val2);
        return paddr_read(val2, 4);
      } else { // 乘号
        return val1 * val2;
      }
    case TK_DIV:
      return val1 / val2;
    case TK_EQ:
      return val1 == val2;
    case TK_LT:
      return val1 < val2;
    case TK_MT:
      return val1 > val2;
    default:
      panic("错误的操作符: %d", tokens[op].type);
      break;
    }
  }
  *success = false;
  return NULL_NUM;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  word_t i = eval(0, nr_token - 1, success);

  // Log("表达式的结果为 %#X", i);
  return i;
}
