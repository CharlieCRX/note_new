#include <regex.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

typedef uint32_t word_t;

/* mock macros */
#define ARRLEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define Log(...) printf(__VA_ARGS__), printf("\n")

#define panic(fmt, ...)           \
  do {                            \
    printf("PANIC: " fmt, ##__VA_ARGS__); \
    assert(0);                    \
  } while (0)
#define TODO() panic("please implement me")

enum {
  TK_NOTYPE = 256, 
  TK_EQ,
  TK_DEC,
  TK_HEX,
  TK_REG,
  TK_VAL
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  {" +", TK_NOTYPE},                   // spaces
  {"0[xX][0-9a-fA-F]+", TK_HEX},       // hex
  {"\\$(0|[a-zA-Z]+[0-9]*)", TK_REG},  // reg
  {"[A-Za-z_][A-Za-z0-9_]*", TK_VAL},  // val
  {"[0-9]+", TK_DEC},   // decimal
  {"\\+", '+',},        // plus
  {"\\-", '-'},         // 减
  {"\\*", '*'},         // 乘
  {"\\/", '/'},         // 除
  {"\\(", '('},         // 左括号
  {"\\)", ')'},         // 右括号
  {"==", TK_EQ},        // equal
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

  for (i = 0; i < NR_REGEX; i ++) {
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
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        if (rules[i].token_type == TK_NOTYPE) {
          break;
        }

        tokens[nr_token].type = rules[i].token_type;

        switch (rules[i].token_type) {
          case TK_DEC: 
          case TK_HEX:
          case TK_REG:
          case TK_VAL:
            strncpy(tokens[nr_token].str, substr_start, substr_len); break;
          default:;
        }
        nr_token++;

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


word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  TODO();

  return 0;
}

// ########################### 可复用测试框架 ###########################
// 测试用例数据结构：封装“输入+预期输出”
typedef struct {
  const char *name;        // 测试用例名称（如 "test1: 1+2+3"）
  const char *expr;        // 测试表达式
  int expected_nr_token;   // 预期 Token 数量
  int *expected_types;     // 预期每个 Token 的类型数组
  const char **expected_strs; // 预期每个 Token 的字符串数组
} TestCase;

// 通用测试函数：所有测试用例复用这个函数
int run_test_case(const TestCase *case_) {
  printf("=== 运行测试：%s ===\n", case_->name);
  memset(tokens, 0, sizeof(tokens));
  
  // 1. 生成 Token（调用你的 make_token）
  int ret = make_token((char*)case_->expr);
  if (!ret) {
    printf("❌ 测试失败：make_token 解析表达式失败！\n");
    return 0; // 测试失败返回 0
  }

  // 2. 断言 Token 数量匹配
  if (nr_token != case_->expected_nr_token) {
    printf("❌ 测试失败：Token 数量不匹配！\n");
    printf("   预期：%d，实际：%d\n", case_->expected_nr_token, nr_token);
    return 0;
  }

  // 3. 遍历校验每个 Token 的类型和字符串
  int pass = 1;
  for (int i = 0; i < nr_token; i++) {
    // 打印当前 Token（和你原测试逻辑一致）
    if (tokens[i].type == TK_DEC || tokens[i].type == TK_HEX || tokens[i].type == TK_REG || tokens[i].type == TK_VAL) {
      printf("token[%d]: type=%d, str=%s\n", i, tokens[i].type, tokens[i].str);
    } else {
      printf("token[%d]: type=%d, str=%c\n", i, tokens[i].type, tokens[i].type);
    }

    // 校验类型
    if (tokens[i].type != case_->expected_types[i]) {
      printf("❌ Token[%d] 类型不匹配！预期：%d，实际：%d\n", 
             i, case_->expected_types[i], tokens[i].type);
      pass = 0;
    }
    // 校验字符串
    if ((tokens[i].type == TK_DEC || tokens[i].type == TK_HEX || tokens[i].type == TK_REG || tokens[i].type == TK_VAL) && strcmp(tokens[i].str, case_->expected_strs[i]) != 0) {
      printf("❌ Token[%d] 字符串不匹配！预期：%s，实际：%s\n", 
             i, case_->expected_strs[i], tokens[i].str);
      pass = 0;
    }
  }

  // 4. 输出测试结果
  if (pass) {
    printf("✅ 测试通过！\n");
    return 1; // 测试成功返回 1
  } else {
    printf("❌ 测试失败！\n");
    return 0;
  }
}

// 测试套件：批量运行所有测试用例
void run_all_tests(TestCase *cases, int n) {
  int success = 0, fail = 0;
  printf("==================== 运行所有测试 ====================\n");
  for (int i = 0; i < n; i++) {
    if (run_test_case(&cases[i])) {
      success++;
    } else {
      fail++;
    }
    printf("----------------------------------------------------\n");
  }
  // 统计结果
  printf("==================== 测试总结 ====================\n");
  printf("✅ 成功：%d，❌ 失败：%d\n", success, fail);
}


int main() {
  init_regex();
	// 测试用例 1：1+2+3
  int types1[] = {TK_DEC, '+', TK_DEC, '+', TK_DEC};
  const char *strs1[] = {"1", "+", "2", "+", "3"};
  TestCase case1 = {
    .name = "test1: 1+2+3",
    .expr = "1+2+3",
    .expected_nr_token = 5,
    .expected_types = types1,
    .expected_strs = strs1
  };

  // 测试用例 2：1  + 2 + 3
  int types2[] = {TK_DEC, '+', TK_DEC, '+', TK_DEC};
  const char *strs2[] = {"1", "+", "2", "+", "3"};
  TestCase case2 = {
    .name = "test2: 1  + 2 + 3",
    .expr = "1  + 2 + 3",
    .expected_nr_token = 5,
    .expected_types = types2,
    .expected_strs = strs2
  };


  // 测试用例3：(1+315)/4- 3*428 + 8
  int types3[] = {'(', TK_DEC, '+', TK_DEC, ')', '/', TK_DEC, '-', TK_DEC, '*', TK_DEC, '+', TK_DEC};
  const char *strs3[] = {"(", "1", "+", "315", ")", "/", "4", "-", "3", "*", "428", "+", "8"};
  TestCase case3 = {
    .name = "test_expr: (1+315)/4- 3*428 + 8",
    .expr = "(1+315)/4- 3*428 + 8",
    .expected_nr_token = 13,
    .expected_types = types3,
    .expected_strs = strs3
  };

  // 测试用例4：(0x1234AfdE + 12345 + 0123) / 0x12345 - 12
  int types_hex_mix[] = {
    '(',        // (
    TK_HEX,     // 0x1234AfdE
    '+',        // +
    TK_DEC,     // 12345
    '+',        // +
    TK_DEC,     // 0123（十进制，无x/X前缀）
    ')',  // )
    '/',        // /
    TK_HEX,     // 0x12345
    '-',        // -
    TK_DEC      // 12
  };
  const char *strs_hex_mix[] = {
    "(",
    "0x1234AfdE",
    "+",
    "12345",
    "+",
    "0123",
    ")",
    "/",
    "0x12345",
    "-",
    "12"
  };
  TestCase case4 = {
    .name = "test_hex_mix: (0x1234AfdE + 12345 + 0123) / 0x12345 - 12",
    .expr = "(0x1234AfdE + 12345 + 0123) / 0x12345 - 12",
    .expected_nr_token = 11,
    .expected_types = types_hex_mix,
    .expected_strs = strs_hex_mix
  };


  // 测试用例：0x80100000+   ($a0 +5)*4 - *(  $t1 + 8) + number
  int types_complex[] = {
    TK_HEX,     // 0x80100000
    '+',        // +
    '(',        // (
    TK_REG,     // $a0
    '+',        // +
    TK_DEC,     // 5
    ')',        // )
    '*',        // *
    TK_DEC,     // 4
    '-',        // -
    '*',     // *（一元解引用）
    '(',  // (
    TK_REG,     // $t1
    '+',        // +
    TK_DEC,     // 8
    ')',  // )
    '+',        // +
    TK_VAL    // number
  };
  const char *strs_complex[] = {
    "0x80100000",
    "+",
    "(",
    "$a0",
    "+",
    "5",
    ")",
    "*",
    "4",
    "-",
    "*",
    "(",
    "$t1",
    "+",
    "8",
    ")",
    "+",
    "number"
  };
  TestCase case5 = {
    .name = "test_complex: 0x80100000+   ($a0 +5)*4 - *(  $t1 + 8) + number",
    .expr = "0x80100000+   ($a0 +5)*4 - *(  $t1 + 8) + number",
    .expected_nr_token = 18,
    .expected_types = types_complex,
    .expected_strs = strs_complex
  };


	// 批量运行所有测试

  // 测试套件：把所有用例放入数组
  TestCase all_cases[] = {case1, case2, case3, case4, case5};
  int n = sizeof(all_cases) / sizeof(all_cases[0]);
  run_all_tests(all_cases, n);

  return 0;
}
