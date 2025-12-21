#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>

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

static Token tokens[16];
static int g_test_token_count;

typedef struct {
  const char *name;      // 描述，如 "1 + 2 * 3"
  Token test_tokens[16]; // 输入序列
  int nr_tokens;         // test_tokens 的长度
  int start;             // 起始地址
  int end;               // 结束地址
  int expected_op_pos;   // 预期主运算符的下标
} MainOpTestCase;

// 辅助函数: 获取 Token 类型的可读名称 (用于报错)
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


// 待测试函数声明
static int find_main_operator(int start, int end);

MainOpTestCase main_op_test_cases[] = {
  // ==================== 绿色场景: 不带括号 ====================
  {
    .name = "不带括号: 1 + 2 + 3 - 4",
    .test_tokens = {
      {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {'+'}, {TK_DEC, "3"}, {'-'}, {TK_DEC, "4"}
    },
    .nr_tokens = 7,
    .start = 0,
    .end = 6,
    .expected_op_pos = 5  // 最右侧的减号是主运算符
  },
  {
    .name = "不带括号: 1 + 2 - 3 / 4",
    .test_tokens = {
      {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {'-'}, {TK_DEC, "3"}, {'/'}, {TK_DEC, "4"}
    },
    .nr_tokens = 7,
    .start = 0,
    .end = 6,
    .expected_op_pos = 3  // 减号优先级低于除号
  },
  {
    .name = "不带括号: 1 + 2 - 3 * 4",
    .test_tokens = {
      {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {'-'}, {TK_DEC, "3"}, {'*'}, {TK_DEC, "4"}
    },
    .nr_tokens = 7,
    .start = 0,
    .end = 6,
    .expected_op_pos = 3  // 减号是主运算符
  },
  {
    .name = "不带括号: 1 + 2 - 3 * 4 + 5",
    .test_tokens = {
      {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {'-'}, {TK_DEC, "3"}, {'*'}, {TK_DEC, "4"}, {'+'}, {TK_DEC, "5"}
    },
    .nr_tokens = 9,
    .start = 0,
    .end = 8,
    .expected_op_pos = 7  // 最右侧的加号
  },
  {
    .name = "不带括号: 1 / 2 * 3 * 4 + 5",
    .test_tokens = {
      {TK_DEC, "1"}, {'/'}, {TK_DEC, "2"}, {'*'}, {TK_DEC, "3"}, {'*'}, {TK_DEC, "4"}, {'+'}, {TK_DEC, "5"}
    },
    .nr_tokens = 9,
    .start = 0,
    .end = 8,
    .expected_op_pos = 7  // 加号优先级最低
  },
  {
    .name = "不带括号: 1 * 2 / 3 * 4 * 5",
    .test_tokens = {
      {TK_DEC, "1"}, {'*'}, {TK_DEC, "2"}, {'/'}, {TK_DEC, "3"}, {'*'}, {TK_DEC, "4"}, {'*'}, {TK_DEC, "5"}
    },
    .nr_tokens = 9,
    .start = 0,
    .end = 8,
    .expected_op_pos = 7  // 同优先级取最右侧乘号
  },

  // ==================== 绿色场景: 带括号 ====================
  {
    .name = "带括号: (1 + 2) + 3 - 4",
    .test_tokens = {
      {'('}, {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {')'}, {'+'}, {TK_DEC, "3"}, {'-'}, {TK_DEC, "4"}
    },
    .nr_tokens = 9,
    .start = 0,
    .end = 8,
    .expected_op_pos = 7  // 括号外最右侧减号
  },
  {
    .name = "带括号: (1 + 2 + 3) - 4",
    .test_tokens = {
      {'('}, {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {'+'}, {TK_DEC, "3"}, {')'}, {'-'}, {TK_DEC, "4"}
    },
    .nr_tokens = 9,
    .start = 0,
    .end = 8,
    .expected_op_pos = 7  // 括号外减号
  },
  {
    .name = "带括号: 1 + (2 - 3) / 4",
    .test_tokens = {
      {TK_DEC, "1"}, {'+'}, {'('}, {TK_DEC, "2"}, {'-'}, {TK_DEC, "3"}, {')'}, {'/'}, {TK_DEC, "4"}
    },
    .nr_tokens = 9,
    .start = 0,
    .end = 8,
    .expected_op_pos = 1  // 加号优先级更低
  },
  {
    .name = "带括号: 1 + 2 - (3 / 4)",
    .test_tokens = {
      {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {'-'}, {'('}, {TK_DEC, "3"}, {'/'}, {TK_DEC, "4"}, {')'}
    },
    .nr_tokens = 9,
    .start = 0,
    .end = 8,
    .expected_op_pos = 3  // 减号是主运算符
  },

  {
    .name = "带括号: 1 + 2 - (3 + 4)",
    .test_tokens = {
      {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {'-'}, {'('}, {TK_DEC, "3"}, {'+'}, {TK_DEC, "4"}, {')'}
    },
    .nr_tokens = 9,
    .start = 0,
    .end = 8,
    .expected_op_pos = 3  // 减号是主运算符
  },

  {
    .name = "带括号: (1 + 2 - 3) * 4",
    .test_tokens = {
      {'('}, {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {'-'}, {TK_DEC, "3"}, {')'}, {'*'}, {TK_DEC, "4"}
    },
    .nr_tokens = 9,
    .start = 0,
    .end = 8,
    .expected_op_pos = 7  // 括号外乘号
  },

    // ==================== 新增1:负数场景 ====================
  {
    .name = "负数场景:-1 + 2 (负号开头)",
    .test_tokens = {
      {'-'}, {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}
    },
    .nr_tokens = 4,
    .start = 0,
    .end = 3,
    .expected_op_pos = 2  // 主运算符是加号 (索引2)
  },
  {
    .name = "负数场景:1 - -2 (表达式中嵌套负数)",
    .test_tokens = {
      {TK_DEC, "1"}, {'-'}, {'-'}, {TK_DEC, "2"}
    },
    .nr_tokens = 4,
    .start = 0,
    .end = 3,
    .expected_op_pos = 1  // 主运算符是第一个减号 (索引1)
  },
  {
    .name = "负数场景:(-1 + 2) * 3 (带括号的负数)",
    .test_tokens = {
      {'('}, {'-'}, {TK_DEC, "1"}, {'+'}, {TK_DEC, "2"}, {')'}, {'*'}, {TK_DEC, "3"}
    },
    .nr_tokens = 8,
    .start = 0,
    .end = 7,
    .expected_op_pos = 6  // 主运算符是乘号 (索引6)
  },
  {
    .name = "负数场景:-0x1A * 2 + 3 (十六进制负数+混合运算)",
    .test_tokens = {
      {'-'}, {TK_HEX, "0x1A"}, {'*'}, {TK_DEC, "2"}, {'+'}, {TK_DEC, "3"}
    },
    .nr_tokens = 6,
    .start = 0,
    .end = 5,
    .expected_op_pos = 4  // 主运算符是加号 (索引4)
  },
  {
    .name = "负数场景:3 + (-2 * 4) (括号内的负数)",
    .test_tokens = {
      {TK_DEC, "3"}, {'+'}, {'('}, {'-'}, {TK_DEC, "2"}, {'*'}, {TK_DEC, "4"}, {')'}
    },
    .nr_tokens = 8,
    .start = 0,
    .end = 7,
    .expected_op_pos = 1  // 主运算符是加号 (索引1)
  },
  {
    .name = "负数场景:5 - (-3 + 1) / 2 (嵌套负数+混合优先级)",
    .test_tokens = {
      {TK_DEC, "5"}, {'-'}, {'('}, {'-'}, {TK_DEC, "3"}, {'+'}, {TK_DEC, "1"}, {')'}, {'/'}, {TK_DEC, "2"}
    },
    .nr_tokens = 10,
    .start = 0,
    .end = 9,
    .expected_op_pos = 1  // 主运算符是第一个减号 (索引1)
  },

  // ==================== 新增:连续负号/标识符/寄存器场景 ====================
  {
    .name = "单目负号:--2 (无双目运算符)",
    .test_tokens = {
      {'-'}, {'-'}, {TK_DEC, "2"}
    },
    .nr_tokens = 3,
    .start = 0,
    .end = 2,
    .expected_op_pos = -1  // 仅单目负号，无主运算符
  },
  {
    .name = "单目负号:--p (标识符，无双目运算符)",
    .test_tokens = {
      {'-'}, {'-'}, {TK_IDENT, "p"}  // 标识符p的str为"p"
    },
    .nr_tokens = 3,
    .start = 0,
    .end = 2,
    .expected_op_pos = -1  // 仅单目负号，无主运算符
  },
  {
    .name = "单目负号:- $a0 (寄存器，无双目运算符)",
    .test_tokens = {
      {'-'}, {TK_REG, "$a0"}  // 寄存器$a0的str为"$a0"
    },
    .nr_tokens = 2,
    .start = 0,
    .end = 1,
    .expected_op_pos = -1  // 仅单目负号，无主运算符
  },
  {
    .name = "混合场景:3 + --2 (连续负号+双目运算)",
    .test_tokens = {
      {TK_DEC, "3"}, {'+'}, {'-'}, {'-'}, {TK_DEC, "2"}
    },
    .nr_tokens = 5,
    .start = 0,
    .end = 4,
    .expected_op_pos = 1  // 主运算符是加号 (索引1)
  },
  {
    .name = "混合场景:$a0 - --p (寄存器+标识符+连续负号)",
    .test_tokens = {
      {TK_REG, "$a0"}, {'-'}, {'-'}, {'-'}, {TK_IDENT, "p"}
    },
    .nr_tokens = 5,
    .start = 0,
    .end = 4,
    .expected_op_pos = 1  // 主运算符是减号 (索引1)
  },

  // ==================== 异常场景: 无主运算符 ====================
  {
    .name = "异常场景: 1 $ 2 (非法运算符 )",
    .test_tokens = {
      {TK_DEC, "1"}, {0}, {TK_DEC, "2"}  // 0=未知类型，无str
    },
    .nr_tokens = 3,
    .start = 0,
    .end = 2,
    .expected_op_pos = -1
  },
  {
    .name = "异常场景: 1 (2 4) (无运算符 )",
    .test_tokens = {
      {TK_DEC, "1"}, {'('}, {TK_DEC, "2"}, {TK_DEC, "4"}, {')'}
    },
    .nr_tokens = 5,
    .start = 0,
    .end = 4,
    .expected_op_pos = -1
  },
  {
    .name = "异常场景: 1 0 (无运算符 )",
    .test_tokens = {
      {TK_DEC, "1"}, {TK_DEC, "0"}
    },
    .nr_tokens = 2,
    .start = 0,
    .end = 1,
    .expected_op_pos = -1
  }
};

// 测试用例总数
#define MAIN_OP_TEST_CASE_COUNT (sizeof(main_op_test_cases) / sizeof(MainOpTestCase))


/**
 * 运行单个主运算符检测测试用例
 * @param case_idx 测试用例索引
 * @return 测试是否通过 (true=通过，false=失败 )
 */
bool run_single_main_op_test(int case_idx) {
  if (case_idx < 0 || case_idx >= MAIN_OP_TEST_CASE_COUNT) {
    printf("❌ 无效的测试用例索引: %d\n", case_idx);
    return false;
  }

  MainOpTestCase *test_case = &main_op_test_cases[case_idx];
  
  // 初始化全局Token环境
  memcpy(tokens, test_case->test_tokens, sizeof(Token) * test_case->nr_tokens);
  g_test_token_count = test_case->nr_tokens;

  // 执行待测试函数
  int actual_op_pos = find_main_operator(test_case->start, test_case->end);

  // 输出测试结果 (仅数字展示str，运算符/括号仅展示type )
  printf("┌─────────────────────────────────────────────────┐\n");
  printf("【测试用例 %d】%s\n", case_idx + 1, test_case->name);
  printf("检测区间: [%d, %d]\n", test_case->start, test_case->end);
  printf("预期主运算符索引: %d\n", test_case->expected_op_pos);
  if (test_case->expected_op_pos != -1 && test_case->expected_op_pos < test_case->nr_tokens) {
    Token exp_token = test_case->test_tokens[test_case->expected_op_pos];
    printf("预期主运算符: %s", get_token_type_name(exp_token.type));
    // 仅数字Token展示str
    if (exp_token.type == TK_DEC || exp_token.type == TK_HEX) {
      printf("(%s)", exp_token.str);
    }
    printf("\n");
  }
  printf("实际返回索引:      %d\n", actual_op_pos);
  if (actual_op_pos != -1 && actual_op_pos < test_case->nr_tokens) {
    Token act_token = tokens[actual_op_pos];
    printf("实际返回运算符: %s", get_token_type_name(act_token.type));
    // 仅数字Token展示str
    if (act_token.type == TK_DEC || act_token.type == TK_HEX) {
      printf("(%s)", act_token.str);
    }
    printf("\n");
  }
  bool passed = (actual_op_pos == test_case->expected_op_pos);
  printf("测试结果:          %s\n", passed ? "✅ 通过" : "❌ 失败");
  printf("└─────────────────────────────────────────────────┘\n\n");

  return passed;
}

/**
 * 运行所有主运算符检测测试用例
 * @return 总通过数
 */
int run_all_main_op_tests() {
  printf("==================== 开始执行所有主运算符检测测试用例 ====================\n\n");
  int passed_count = 0;

  for (int i = 0; i < MAIN_OP_TEST_CASE_COUNT; i++) {
    if (run_single_main_op_test(i)) {
      passed_count++;
    }
  }

  // 测试汇总
  printf("==================== 测试汇总 ====================\n");
  printf("总用例数: %d\n", MAIN_OP_TEST_CASE_COUNT);
  printf("通过数:    %d\n", passed_count);
  printf("失败数:    %d\n", MAIN_OP_TEST_CASE_COUNT - passed_count);
  printf("通过率:    %.2f%%\n", (float)passed_count / MAIN_OP_TEST_CASE_COUNT * 100);

  return passed_count;
}

bool is_operator(int type) {
  switch (type)
  {
  case '+':
  case '-':
  case '*':
  case '/':
    return true;
  default:
    return false;
  }
}

// 判断运算符op2的优先级是否高于op1
bool has_higher_precedence(int op1, int op2) {
  // op1是加减时
  if (op1 == '+' || op1 == '-') {
    if (op2 == '+' || op2 == '-') {
      return true;
    } else {
      return false;
    }
  } else if (op1 == '*' || op1 == '/') {
    return true;
  }

  return false;
}

// 二元运算符探测器
static int find_main_operator1(int start, int end) {
  int op_index = -1;
  if (start > end) {
    return op_index;
  }

  int balance = 0;
  for (int i = start; i <= end; i++) {
    // 如果 token[i] 是不在括号内的运算符
    if (balance == 0 && is_operator(tokens[i].type)) {

      if (op_index == -1 || has_higher_precedence(tokens[op_index].type, tokens[i].type)) {
        op_index = i;
        continue;
      }
    } else if (tokens[i].type == '(') { // 括号内的运算符均不做数
      balance++;
    } else if (tokens[i].type == ')') {
      balance--;
    }
  }

  return op_index;
}

// 是否能作为一个二元运算符的前缀
bool can_be_binary_prefix(int i, int start) {
  if (i < start) {
    return false;
  }

  // 前一个 Token 是否是一个操作数的合法结束标志。
  int type = tokens[i].type;
  switch (type)
  {
  case TK_DEC:
  case TK_HEX:
  case TK_IDENT:
  case TK_REG:
  case ')':
    return true;
  
  default:
    return false;
  }
}

static int find_main_operator(int start, int end) {

  int op_index = -1;
  if (start > end) {
    return op_index;
  }

  int balance = 0;
  for (int i = start; i <= end; i++) {
    // 排除括号内的运算符
    if (tokens[i].type == '(') {
      balance++;
      continue;
    }

    if (tokens[i].type == ')') {
      balance--;
      continue;
    }

    if (balance == 0 && is_operator(tokens[i].type) && can_be_binary_prefix(i - 1, start)) {

      if (op_index == -1 || has_higher_precedence(tokens[op_index].type, tokens[i].type)) {
        op_index = i;
      }
    }
  }

  return op_index;
}

// 示例主函数
int main() {
  // 运行单个测试用例 (第1个 )
  printf("---------- 运行单个测试用例 ----------\n");
  run_single_main_op_test(0);

  // 运行所有测试用例
  printf("\n---------- 运行所有测试用例 ----------\n");
  run_all_main_op_tests();

  time_t rawtime;
  struct tm *timeinfo;
  char buffer[80];

  // 获取当前时间
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // 使用strftime格式化时间
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  printf("当前时间: %s\n", buffer);

  return 0;
}