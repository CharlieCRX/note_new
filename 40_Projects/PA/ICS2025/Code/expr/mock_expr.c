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

#define MAX_TOKEN_STR_LEN 32

enum {
  TK_NOTYPE = 256, 
  TK_EQ,
  TK_DEC,
  TK_HEX,
  TK_REG,
  TK_IDENT
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  {" +", TK_NOTYPE},                   // spaces
  {"0[xX][0-9a-fA-F]+", TK_HEX},       // hex
  {"\\$(0|[a-zA-Z]+[0-9]*)", TK_REG},  // reg
  {"[A-Za-z_][A-Za-z0-9_]*", TK_IDENT},  // 标识符
  {"[0-9]+", TK_DEC},   // decimal
  {"\\+", '+',},        // plus
  {"\\-", '-'},         // 减
  {"\\*", '*'},         // 乘
  {"\\/", '/'},         // 除
  {"\\(", '('},         // 左括号
  {"\\)", ')'},         // 右括号
  {"==", TK_EQ},        // equal
};

// 辅助函数：获取 Token 类型的可读名称 (用于报错)
static const char* get_token_type_name(int token_type) {
  switch (token_type) {
    case TK_DEC:    return "TK_DEC(十进制数字)";
    case TK_HEX:    return "TK_HEX(十六进制数字)";
    case TK_REG:    return "TK_REG(寄存器)";
    case TK_IDENT:  return "TK_IDENT (标识符)";
    case '+':       return "加号";
    case '-':       return "减号/负号";
    case '*':       return "乘号/解引用";
    case '/':       return "除号";
    case '(':       return "左括号";
    case ')':       return "右括号";
    case TK_EQ:     return "TK_EQ (等于号)";
    default:        return "未知类型";
  }
}

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
  char str[MAX_TOKEN_STR_LEN];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

#define TOKENS_LEN ARRLEN(tokens)

// 辅助函数：检查 Token 长度(严格模式)
static bool check_token_length(int token_type, 
                               const char *substr_start, 
                               int substr_len, 
                               const char *expr, 
                               int current_position);

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

        // ========== 新增1：Token数组边界检查（核心！写入前拦截越界） ==========
        if (nr_token >= TOKENS_LEN) {
          // 专属错误提示：明确是Token数量超限，而非无匹配规则
          fprintf(stderr, "❌ Token数组溢出错误：\n");
          fprintf(stderr, "  - 数组最大容量：%d，当前已生成Token数：%d\n", TOKENS_LEN, nr_token);
          fprintf(stderr, "  - 出错位置：表达式第 %d 个字符（内容：%.*s）\n", position - substr_len, substr_len, substr_start);
          fprintf(stderr, "  - 完整表达式：%s\n", e);
          fprintf(stderr, "  - 错误标记：%.*s^\n", position - substr_len, "");
          return false; // 终止解析，避免段错误
        }

        // ========== 调用抽象函数：检查 Token 长度 ==========
        if (!check_token_length(rules[i].token_type, substr_start, substr_len, e, position)) {
          return false; // 长度超限，终止解析
        }

        tokens[nr_token].type = rules[i].token_type;

        switch (rules[i].token_type) {
          case TK_DEC: 
          case TK_HEX:
          case TK_REG:
          case TK_IDENT:
            strncpy(tokens[nr_token].str, substr_start, substr_len); 
            tokens[nr_token].str[substr_len] = '\0'; // 补结束符（避免溢出）
            break;
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

// 检查 Token 长度 (严格模式)
// 返回值：true = 长度合法；false = 长度超限 (需终止解析)
static bool check_token_length(int token_type, 
                               const char *substr_start, 
                               int substr_len, 
                               const char *expr, 
                               int current_position) {
  // 步骤1：仅对需要存储字符串的 Token 做长度检查 (运算符/括号无需检查)
  bool need_check = (token_type == TK_DEC || 
                     token_type == TK_HEX || 
                     token_type == TK_REG || 
                     token_type == TK_IDENT);
  if (!need_check) {
    return true; // 无需检查，直接返回合法
  }

  // 步骤2：严格检查长度是否超限
  if (substr_len > MAX_TOKEN_STR_LEN) {
    // 步骤3：打印结构化错误信息 (精准定位问题)
    fprintf(stderr, "❌ Token 过长错误 (严格模式)：\n");
    fprintf(stderr, "  - Token 类型：%s\n", get_token_type_name(token_type));
    fprintf(stderr, "  - 实际长度：%d (上限：%d)\n", substr_len, MAX_TOKEN_STR_LEN);
    fprintf(stderr, "  - 起始位置：%d\n", current_position - substr_len); // 计算Token起始位置
    fprintf(stderr, "  - 超限内容：%.*s\n", substr_len, substr_start);
    fprintf(stderr, "  - 完整表达式：%s\n", expr);
    fprintf(stderr, "  - 错误标记：%.*s^\n", current_position - substr_len, "");
    return false; // 长度超限，返回不合法
  }

  // 步骤4：长度合法，返回true
  return true;
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
    if (tokens[i].type == TK_DEC || tokens[i].type == TK_HEX || tokens[i].type == TK_REG || tokens[i].type == TK_IDENT) {
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
    if ((tokens[i].type == TK_DEC || tokens[i].type == TK_HEX || tokens[i].type == TK_REG || tokens[i].type == TK_IDENT) && strcmp(tokens[i].str, case_->expected_strs[i]) != 0) {
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
    TK_IDENT    // number
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

  // ===================== 超长测试用例 =====================
// 测试场景：覆盖十六进制/十进制/寄存器/标识符/一元运算符/二元运算符/括号嵌套/混合空格
int types_long[] = {
  '(',         // 0: (
  '(',         // 1: (
  TK_HEX,      // 2: 0x80100000
  '+',         // 3: +
  TK_REG,      // 4: $a0
  ')',         // 5: )
  '*',         // 6: *
  TK_DEC,      // 7: 456789
  '-',         // 8: -
  '-',         // 9: -（一元负号）
  TK_HEX,      // 10: 0x7FFFFFFF
  '/',         // 11: /
  '(',         // 12: (
  TK_REG,      // 13: $t1
  '+',         // 14: +
  TK_DEC,      // 15: 8
  ')',         // 16: )
  TK_EQ,       // 17: ==
  '*',         // 18: *（一元解引用）
  '(',         // 19: (
  TK_REG,      // 20: $ra
  '+',         // 21: +
  TK_REG,      // 22: $s10
  ')',         // 23: )
  '+',         // 24: +
  TK_IDENT,    // 25: number1234567890abcdefghijklm
  '+',         // 26: +
  TK_DEC,      // 27: 987654321012345
  '+',         // 28: +
  TK_REG,      // 29: $0
  '-',         // 30: -
  TK_DEC,      // 31: 12345678901234567890
  '+',         // 32: +
  TK_HEX,      // 33: 0x123456789ABCDEF01234
  '+',         // 34: +
  TK_IDENT,    // 35: ident_long_1234567890abcdefghij
  '+',         // 36: +
  '(',         // 37: (
  '(',         // 38: (
  '(',         // 39: (
  TK_REG,      // 40: $s5
  '+',         // 41: +
  TK_DEC,      // 42: 789
  ')',         // 43: )
  '*',         // 44: *
  TK_DEC,      // 45: 10
  ')',         // 46: )
  '/',         // 47: /
  '-',         // 48: -（一元负号）
  TK_DEC,      // 49: 987
  ')',         // 50: )
  TK_EQ,       // 51: ==
  TK_REG,      // 52: $t2
  '+',         // 53: +
  TK_DEC,      // 54: 88888888888888888888
  ')'          // 55: )
};

const char *strs_long[] = {
  "(",
  "(",
  "0x80100000",
  "+",
  "$a0",
  ")",
  "*",
  "456789",
  "-",
  "-",
  "0x7FFFFFFF",
  "/",
  "(",
  "$t1",
  "+",
  "8",
  ")",
  "==",
  "*",
  "(",
  "$ra",
  "+",
  "$s10",
  ")",
  "+",
  "number1234567890abcdefghijklm",
  "+",
  "987654321012345",
  "+",
  "$0",
  "-",
  "12345678901234567890",
  "+",
  "0x123456789ABCDEF01234",
  "+",
  "ident_long_1234567890abcdefghij",
  "+",
  "(",
  "(",
  "(",
  "$s5",
  "+",
  "789",
  ")",
  "*",
  "10",
  ")",
  "/",
  "-",
  "987",
  ")",
  "==",
  "$t2",
  "+",
  "88888888888888888888",
  ")"
};

TestCase case_long = {
  .name = "test_long: 全场景覆盖超长表达式（括号嵌套+一元运算符+多类型Token）",
  .expr = "((0x80100000 +   $a0) * 456789 - -0x7FFFFFFF / ($t1 + 8) == *(  $ra + $s10 ) + number1234567890abcdefghijklm + 987654321012345 + $0 - 12345678901234567890 + 0x123456789ABCDEF01234 + ident_long_1234567890abcdefghij + ((($s5 + 789) * 10) / -987) == $t2 + 88888888888888888888)",
  .expected_nr_token = 56,
  .expected_types = types_long,
  .expected_strs = strs_long
};

// ===================== 扩展：超长Token超限测试用例（严格模式） =====================
// 用于验证Token过长的错误处理逻辑（可选）
int types_too_long[] = {
  TK_HEX,      // 0: 0x80100000801000008010000080100000（长度36 > MAX_TOKEN_STR_LEN=32）
  '+',         // 1: +
  TK_IDENT     // 2: ident_1234567890abcdefghijklmnopqrstuvwxyz（长度33 > 32）
};

const char *strs_too_long[] = {
  "0x80100000801000008010000080100000",
  "+",
  "ident_1234567890abcdefghijklmnopqrstuvwxyz"
};

TestCase case_too_long = {
  .name = "test_too_long: 超长Token超限测试（严格模式）",
  .expr = "0x80100000801000008010000080100000 + ident_1234567890abcdefghijklmnopqrstuvwxyz",
  .expected_nr_token = 3, // 理论上不会走到这一步，严格模式会直接报错终止
  .expected_types = types_too_long,
  .expected_strs = strs_too_long
};


// ----------------- 检测括号匹配度 ----------------


int main() {
  init_regex();
	// 批量运行所有测试

  // 测试套件：把所有用例放入数组
  TestCase all_cases[] = {case1, case2, case3, case4, case5, case_long, case_too_long};
  int n = sizeof(all_cases) / sizeof(all_cases[0]);
  run_all_tests(all_cases, n);

  return 0;
}
