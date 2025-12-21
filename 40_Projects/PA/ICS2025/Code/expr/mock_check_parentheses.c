#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* --- 模拟宏与类型定义 --- */
#define ARRLEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MAX_TOKEN_STR_LEN 32

enum {
  TK_NOTYPE = 256, TK_EQ, TK_DEC, TK_HEX, TK_REG, TK_IDENT
};

typedef struct token {
  int type;
  char str[MAX_TOKEN_STR_LEN];
} Token;

static Token tokens[32] = {};

/* --- 括号检查错误类型 --- */
typedef enum {
  PAREN_ERR_NONE,
  PAREN_ERR_MISMATCH,    // 括号数量或顺序不匹配
  PAREN_ERR_EMPTY,       // 空括号 ()
  PAREN_ERR_NOT_WRAP,    // 不是(expr)类型：(1+2)-(3)或者 )1-2(
  PAREN_ERR_INVALID_RANGE
} ParenErrType;

static const char* paren_err_msg[] = {
  [PAREN_ERR_NONE]          = "无错误",
  [PAREN_ERR_MISMATCH]      = "括号不匹配",
  [PAREN_ERR_EMPTY]         = "空括号",
  [PAREN_ERR_NOT_WRAP]      = "非单一包裹结构",
  [PAREN_ERR_INVALID_RANGE] = "无效范围"
};

/* --- 核心函数：check_parentheses --- */
/**
 * @brief 检查 [start, end] 范围内的 token 是否构成一个合法的 (expr) 结构
 * 逻辑：
 * 1. 首尾必须是 '(' 和 ')'
 * 2. 遍历过程中，左括号必须与右括号抵消
 * 3. 核心判定：在到达最后一个 token 前，括号 balance 不能提前归零（否则说明是 (a+b)+(c+d) 结构）
 */
static bool check_parentheses(int start, int end, ParenErrType *err_type) {
  *err_type = PAREN_ERR_NONE;

  if (start < 0 || end < 0 || start > end) {
    *err_type = PAREN_ERR_INVALID_RANGE;
    return false;
  }

  // 场景：首尾不匹配（直接判定不是 (expr)）
  if (tokens[start].type != '(' || tokens[end].type != ')') {
    *err_type = PAREN_ERR_NOT_WRAP;
    return false;
  }

  // 场景：空括号 ()
  if (start + 1 == end) {
    *err_type = PAREN_ERR_EMPTY;
    return false;
  }

  int balance = 0;
  for (int i = start; i <= end; i++) {
    if (tokens[i].type == '(') {
      balance++;
    } else if (tokens[i].type == ')') {
      balance--;

      // 场景：中途右括号多于左括号，如 (a+b))+(c
      if (balance < 0) {
        *err_type = PAREN_ERR_MISMATCH;
        return false;
      }

      // 核心判定：如果 balance 提前归零且还没到 end，说明最外层括号没包住全段
      // 例如：(1+2) + (3+4)，当处理到第一个 ')' 时 balance 为 0，但 i < end
      if (balance == 0 && i < end) {
        *err_type = PAREN_ERR_NOT_WRAP;
        return false;
      }
    }
  }

  // 最终 balance 必须为 0（处理多出左括号的情况）
  if (balance != 0) {
    *err_type = PAREN_ERR_MISMATCH;
    return false;
  }

  return true;
}

/* --- 测试系统 --- */
typedef struct {
  const char *name;
  Token test_tokens[16];
  int nr_tokens;
  int start;
  int end;
  bool expected_ret;
  ParenErrType expected_err;
} TestCase;

void run_test(TestCase *c) {
  memset(tokens, 0, sizeof(tokens));
  memcpy(tokens, c->test_tokens, sizeof(Token) * c->nr_tokens);

  ParenErrType err;
  bool ret = check_parentheses(c->start, c->end, &err);

  printf("[%s] -> ", c->name);
  if (ret == c->expected_ret && err == c->expected_err) {
    printf("PASS\n");
  } else {
    printf("FAIL (预期 ret:%d err:%d, 实际 ret:%d err:%d)\n", 
           c->expected_ret, c->expected_err, ret, err);
  }
}

int main() {
  TestCase cases[] = {
    {
      .name = "合法包裹 (1+2)",
      .test_tokens = {{'(', "("}, {TK_DEC, "1"}, {'+', "+"}, {TK_DEC, "2"}, {')', ")"}},
      .nr_tokens = 5, .start = 0, .end = 4,
      .expected_ret = true, .expected_err = PAREN_ERR_NONE
    },
    {
      .name = "非包裹结构 (1+2)+(3+4)",
      .test_tokens = {{'(', "("}, {TK_DEC, "1"}, {')', ")"}, {'+', "+"}, {'(', "("}, {TK_DEC, "3"}, {')', ")"}},
      .nr_tokens = 7, .start = 0, .end = 6,
      .expected_ret = false, .expected_err = PAREN_ERR_NOT_WRAP
    },
    {
      .name = "不匹配-多左 ((1+2)",
      .test_tokens = {{'(', "("}, {'(', "("}, {TK_DEC, "1"}, {')', ")"}},
      .nr_tokens = 4, .start = 0, .end = 3,
      .expected_ret = false, .expected_err = PAREN_ERR_MISMATCH
    },
    {
      .name = "空括号 ()",
      .test_tokens = {{'(', "("}, {')', ")"}},
      .nr_tokens = 2, .start = 0, .end = 1,
      .expected_ret = false, .expected_err = PAREN_ERR_EMPTY
    },
    {
      .name = "纯数字无括号 1+2",
      .test_tokens = {{TK_DEC, "1"}, {'+', "+"}, {TK_DEC, "2"}},
      .nr_tokens = 3, .start = 0, .end = 2,
      .expected_ret = false, .expected_err = PAREN_ERR_NOT_WRAP
    }
  };

  printf("开始括号检测测试：\n");
  for (int i = 0; i < ARRLEN(cases); i++) {
    run_test(&cases[i]);
  }

  return 0;
}