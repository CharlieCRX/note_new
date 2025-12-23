# 表达式求值（二）：语法分析

语法分析的目的就是在上一步的词法分析结束后，对生成的 token 表达式进行语法上的校验分析，检查是否为合法表达式。

这节就主要讲解如何正确地进行语法分析。最终要实现的就是`eval(p, q)`表达式：

```C
eval(p, q):
  if p > q:
    error

  if p == q:
    return 基本值

  if 被一对最外层括号包裹:
    return eval(p+1, q-1)

  op = 寻找主运算符(p, q)

  val1 = eval(p, op-1)
  val2 = eval(op+1, q)

  return apply(op, val1, val2)
```

## 1. 表达式的归纳定义

表达式的归纳定义特性可以浓缩为两个关键点：

1. **基础边界**：存在不可再分的 “原子表达式”（数字、寄存器等），是所有复杂表达式的起点；
2. **递归组合**：用运算符 / 括号将已定义的表达式组合成新表达式，且组合过程可无限套用但总能拆解回基础单元。

## 2. 判断表达式被一对最外层括号包裹

判断的函数名为：`check_parentheses`

### 2.1 核心问题

`check_parentheses` 函数要处理：

1. 是否是合法的 `(expr)` 结构（布尔值）
2. 区分 “括号本身非法” 和 “括号匹配但非 `(expr)` 结构” 两种错误

解决方案：

- **扩展函数接口，通过输出型参数传递错误类型 / 报错信息**，而非仅返回布尔值

### 2.2 基于 TDD 的测试开发流程

TDD（测试驱动开发）的核心是 “先写测试用例→验证失败→实现功能→验证通过”，针对 `check_parentheses` 函数，我们需设计**覆盖所有核心场景**的表驱动测试用例，既验证 “合法 (expr) 结构” 的正确识别，也验证各类错误场景的精准报错（错误类型 + 位置）。

我们首先创建`mock_check_parentheses.c`文件，专门用来测试函数`check_parentheses`。

#### 1. TDD 设计核心思路

针对括号检测函数，测试用例需满足：

- **全覆盖**：覆盖 “合法结构 + 所有错误类型”，无遗漏核心场景；
- **可验证**：每个用例明确 “输入（token 序列 + 检查范围）+ 预期输出（返回值 + 错误类型 + 错误位置）”；
- **低耦合**：测试用例不依赖求值逻辑，仅针对 `check_parentheses` 函数独立验证；
- **结构化**：表驱动设计，新增场景只需补充数据，无需修改测试逻辑。

#### 2. 测试用例数据结构设计

基于 `check_parentheses` 的接口，定义专属测试用例结构体，封装 “输入 + 预期输出”：

```C
// 括号检测测试用例结构体
typedef struct {
  const char *name;				 		// 测试用例名称（便于定位失败场景）
  Token test_tokens[16];			    // 测试用的token序列
  int nr_tokens;						// token序列长度
  int start;							// check_parentheses的start参数
  int end;								// check_parentheses的end参数
  bool expected_ret;					// 预期返回值（true/false）
  ParenErrType expected_err;			// 预期错误类型
} TestCase;
```

#### 3. 核心测试场景与表驱动用例

覆盖 `check_parentheses` 的所有核心场景，每个用例对应一个明确的测试目标：

| 测试场景                 | 用例名称                     | 核心验证点                                                  |
| ------------------------ | ---------------------------- | ----------------------------------------------------------- |
| 合法 (expr) 结构         | test_valid_single_paren      | 返回 true，错误类型为 PAREN_ERR_NONE                        |
| 括号不匹配 - 多左括号    | test_err_mismatch_more_left  | 返回 false，错误类型 PAREN_ERR_MISMATCH，位置为第一个左括号 |
| 括号不匹配 - 多右括号    | test_err_mismatch_more_right | 返回 false，错误类型 PAREN_ERR_MISMATCH，位置为多余的右括号 |
| 括号不匹配 - 顺序错误    | test_err_mismatch_order      | 返回 false，错误类型 PAREN_ERR_MISMATCH，位置为非法右括号   |
| 空括号                   | test_err_empty_paren         | 返回 false，错误类型 PAREN_ERR_EMPTY，位置为左括号          |
| 括号匹配但非 (expr) 结构 | test_err_not_wrap_paren      | 返回 false，错误类型 PAREN_ERR_NOT_WRAP，位置为减号         |
| 无效检查范围             | test_err_invalid_range       | 返回 false，错误类型 PAREN_ERR_INVALID_RANGE，位置 - 1      |

#### 4. 通用测试函数（TDD 核心：自动化验证）

编写可复用的测试函数，自动执行每个用例并验证结果，输出清晰的失败 / 通过信息：

```C
/**
 * @brief 运行单个括号检测测试用例
 * @param case_: 测试用例
 * @return true: 测试通过；false: 测试失败
 */
static bool run_paren_test_case(const ParenTestCase *case_) {

  // 步骤1：替换全局tokens数组（模拟测试场景）
  memcpy(tokens, case_->test_tokens, sizeof(Token) * case_->nr_test_tokens);
  nr_token = case_->nr_test_tokens;

  // 步骤2：调用check_parentheses
  bool actual_ret = check_parentheses(case_->check_start, case_->check_end, &err_type, &err_pos);

  // 步骤3：验证结果（TDD核心：比对预期与实际）
  // 验证返回值
  // 验证错误类型
  // 验证错误位置

  // 步骤4：输出结果
}

/**
 * @brief 运行所有括号检测测试用例
 */
static void run_all_paren_tests() {
  ParenTestCase all_cases[] = {case1, case2, case3, case4, case5, case6, case7};
  ...
}

```

#### 5. TDD 执行流程（落地步骤）

1. **第一步：编写测试用例**（如上），此时 `check_parentheses` 未实现或实现不完整，运行测试会全部失败；
2. **第二步：实现 `check_parentheses` 核心逻辑**，先聚焦 “合法结构” 和 “括号不匹配” 场景；
3. **第三步：运行测试**，修复失败的用例（如补充空括号、非 (expr) 结构的判断）；
4. **第四步：迭代优化**，直到所有测试用例通过，确保函数覆盖所有场景。

### 2.3 check_parentheses 输入输出

#### 1. 核心目标

函数要回答两个关键问题：

- 「是 / 否」：`[start, end]` 范围内的 Token 序列是否是**单一、完整的 `(expr)` 结构**（即整个范围以 `(` 开头、`)` 结尾，且内部是合法表达式，无多余括号 / 其他结构）；
- 「错在哪」：若非法，要区分是 “括号本身不匹配”“空括号”“非 (expr) 结构” 还是 “范围无效”，并定位错误位置。

为了让这个函数“干净”，明确排除这些事情：

- 不检查表达式是否合法

  ```C
  (1 + *)
  (+ 1 2)
  ```

-  不检查是否能求值

  ```C
  (1 / 0)
  ```

#### 2. 输入输出设计

- 输入：检查范围 `[start, end]`（Token 数组的索引）；
- 输出：
  - 返回值：`true`（合法 `(expr)`）/`false`（非法）；
  - 输出型参数 `err_type`：返回具体错误类型（枚举），解决 “报错原因不同” 的核心需求；
  - 输出型参数 `err_pos`：返回错误发生的 Token 位置，方便上层定位问题。

### 2.4 设计步骤拆解

#### 步骤 1：初始化 + 边界校验（兜底，避免无效计算）

- 过滤错误边界：
  - start > end
- 是不是想成为 `(expr)`？
  - 判断表达式首尾是否为左右括号

#### 步骤 2：括号平衡检测（解决 “括号本身是否合法”）

这是括号检测的核心基础，这里我们使用一个括号计数器：

```C
balance = 0   // 当前左括号数量 - 右括号数量
```

这一步只是用来检验**括号是否合法**：

- 遍历中`balance < 0`，说明右括号提前出现
- 遍历结束后，`balance != 0`，说明左括号数量大于右括号

#### 步骤3：判断括号是否提前闭合

判断“最外层是否真的包住整个表达式”，这一步骤需要识别非`"(expr)"`的表达式。类似：

```C
"(4 + 3) * (2 - 1)"
```

- 最外层 `'('` 在 `"(4 + 3)"` 就已经闭合了
- 后面还有内容 → 不算“整体被包住”

伪代码：

```python
balance = 0

for i from 0 to expr.length - 2:   // 注意：不看最后一个 ')'
    if expr[i] == '(':
        balance += 1
    else if expr[i] == ')':
        balance -= 1

    // 如果在中途就回到 0
    // 说明最外层括号提前闭合
    if balance == 0:
        return false
```

#### 步骤 4：返回合法结果（所有校验通过）

若上述所有校验都通过，说明 `[start, end]` 是合法的 `(expr)` 结构，返回 `true`，错误类型为 “无错误”。

#### 步骤 5：错误信息精准传递（核心需求落地）

每个错误场景都对应唯一的 `err_type` 和精准的 `err_pos`：

| 错误类型                    | err_type                | err_pos 定位逻辑                        |
| --------------------------- | ----------------------- | --------------------------------------- |
| 范围无效                    | PAREN_ERR_INVALID_RANGE | -1（无具体位置）                        |
| 括号不匹配（多右 / 顺序错） | PAREN_ERR_MISMATCH      | 错误的右括号位置                        |
| 括号不匹配（多左）          | PAREN_ERR_MISMATCH      | 第一个未匹配的左括号位置                |
| 空括号                      | PAREN_ERR_EMPTY         | 左括号位置                              |
| 非 (expr) 结构              | PAREN_ERR_NOT_WRAP      | 非括号的起始位置或结束位置或(1+2)-(3+4) |

`check_parentheses`括号处理逻辑实现：📄 [`Code/expr/mock_check_parentheses.c`](../../Code/expr/mock_check_parentheses.c)

## 3.找到表达式的主运算符

### 3.1 为什么要找主运算符

> "主运算符"为表达式人工求值时, 最后一步进行运行的运算符, 它指示了表达式的类型(例如当一个表达式的最后一步是减法运算时, 它本质上是一个减法表达式).

找到主运算符号才能运算表达式的结果。

### 3.2 获取主运算符的输入输出特性分析

根据伪代码的指示：

```C
eval(p, q) {
  if (p > q) {
    /* Bad expression */
  }
  else if (p == q) {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */
  }
  else if (check_parentheses(p, q) == true) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return eval(p + 1, q - 1);
  }
  else {
    op = the position of 主运算符 in the token expression;
    val1 = eval(p, op - 1);
    val2 = eval(op + 1, q);

    switch (op_type) {
      case '+': return val1 + val2;
      case '-': /* ... */
      case '*': /* ... */
      case '/': /* ... */
      default: assert(0);
    }
  }
}
```

按照“寻找优先级最低”的原则，确定寻找 **二元主运算符** 函数的输入输出：

#### 1. 输入

输入 token 序列的两个索引地址参数：

- 起始位置：`p`
- 结束位置：`q`

对应了 token 表达式数组要检测的索引区间。

#### 2. 输出

输出的就是主运算符在 token 序列`[p, q]`中的索引。

需要注意的是，函数并不检测主运算符是否合法。

函数声明为：

```C
static int find_main_operator(int start, int end)
```

### 3.3 TDD测试开发流程

#### 1. 测试用例数据结构设计

- 输入：一个 token 数组以及起始和结束地址。
- 输出：验证函数找到的主运算符是否正确。

```C
typedef struct {
    const char *name;          // 用例描述
    Token tokens[16];          // 输入token序列
    int nr_tokens;             // token序列长度
    int start;                 // 检测起始索引（修正原笔误strat）
    int end;                   // 检测结束索引
    int expected_op_pos;       // 预期主运算符索引（-1表示无主运算符）
} MainOpTestCase;
```

#### 2. 核心测试场景

- 通过（绿灯）场景：

  ```tex
  // 不带括号的场景
  1 + 2 + 3 - 4
  1 + 2 - 3 / 4
  1 + 2 - 3 * 4
  1 + 2 - 3 * 4 + 5
  1 / 2 * 3 * 4 + 5
  1 * 2 / 3 * 4 * 5
      
  // 带括号的场景
  (1 + 2) + 3 - 4
  (1 + 2 + 3) - 4
  1 + (2 - 3) / 4
  1 + 2 - (3 + 4)
  (1 + 2 - 3) * 4
  ```

- 异常（红灯）场景：

  ```tex
  // 没有运算符
  1 $ 2
  1 (2 4)
  1 0
  ```

这样经过简单 TDD 测试案例，可以较为简单地完成仅有二元运算符的寻找主运算符的方法（只看符号本身即可）：

```C
static int find_main_operator(int start, int end) {
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
```

### 3.4 单目运算符

当我们引入负数时，发现 `-` 既可以是**减法**（Binary），也可以是**负号**（Unary）。上面实现了双目运算符`+ - * /`的运算规则，那么考虑下如何实现单目运算符：

- 负号（先解决）
- `$`：访问寄存器或内部变量
- `*`：解引用
- `&`：取地址

#### 1. 如何区分负号和减号 🎭

首先举例减号和负号的场景，然后分析不同的特性：

```
// 减号
1 - 2				// 1. 减号的左边 token 为数字、标识符
(1 + 2) - 3			// 2. 减号的左边 token 为右括号
1 + 2 - 3			
(1 + 2) - (3 + 4)

// 负号
1 + -2				// 1. 负号的左边是加号运算符，右边是表达式
(1 + 2) + -3
1 + 2 + -3
-1 + -2				// 2. 负号的左边没有 token，右边是表达式
--1 + -2			// 3. 负号左边是减号运算符，右边是表达式
```

- **减号 (Subtraction):** 二元运算符 (Binary Operator)，需要两个操作数（左边一个，右边一个）。

- **负号 (Negation):** 一元运算符 (Unary Operator)，只需要一个操作数（在右边）。

再归纳总结：

| **当前符号 - 的前一个元素**    | **- 的身份**             | **例子**                      |
| ------------------------------ | ------------------------ | ----------------------------- |
| 数字、标识符、右括号 `)`       | **二元运算符** (减法 ➖)  | `5 - 3`, `x - 1`, `(a+b) - c` |
| 运算符、左括号 `(`、或处于句首 | **一元运算符** (负号 负) | `-5`, `3 * -2`, `(-x)`        |

我们意识到不能只看符号本身，必须看它的**上下文**。 

#### 2. 单目运算符的识别时机

可以通过下面的案例来思考：

```text
*ptr + 5
-5 + 2
```

| **处理顺序** | **表现**                                                     | **结论**     |
| ------------ | ------------------------------------------------------------ | ------------ |
| **单目优先** | 可能会错误地截获属于二元运算的一部分（如把 `*ptr + 5` 误判为 `*(ptr+5)`）。 | ❌ 破坏优先级 |
| **二元优先** | 先找优先级最低的“缝隙”（主运算符）。如果找不到，说明剩下的部分可能是一个整体，再由单目逻辑处理。 | ✅ 保护优先级 |

所以我们选择 二元优先 的处理策略。

#### 3.纯粹的“二元运算符探测器”

之前的寻找主运算符函数`find_main_operator`仅仅是为了服务于二元运算符的结构，寻找一个“主运算符”把表达式劈成左右两半（val1`op`val2）。

而实现这个简单的功能，仅需要考虑符号本身以及其对应的优先级即可。对应的函数`find_main_operator`的辅助函数仅有以下两个函数支持即可：

```C
// 判断是否为运算符
bool is_operator(int type);

// 判断运算符op2的优先级是否高于op1
bool has_higher_precedence(int op1, int op2);
```

现在由于一元运算符的加入，同时要保持`find_main_operator`依旧能够识别到正确的二元运算符，就不仅仅需要支持分割二元运算符的：

1. 符号本身是否为运算符
2. 符号的优先级

还需要加入**上下文**的判断（以负号`-`为例）：

- 如果 `-` 前面是数字、标识符、寄存器或右括号 `)` → **二元减法**。
- 如果 `-` 处于句首，或者前面是左括号 `(` 或另一个运算符 → **一元负号**。

通过加入上下文的判断，将`find_main_operator`打造成为了纯粹的**“二元运算符探测器”**。

通过改造后的`find_main_operator`，学会了“识别身份”。如果发现某个 `-` 或 `*` 处于一元运算符的位置（比如在句首），它会**跳过**。

> 需要注意的是，在修改任何代码之前，请基于 TDD 的开发模式，加入红色🍎的测试案例，让代码不通过，然后再编写代码让其变为:green_apple:通过的测试案例。

这样预想的流程就是：

1. **括号剥离**：处理 `(expr)`，消除最高优先级的边界。
2. **二元主运算符探测**：调用 `find_main_operator`。
   - **关键改进**：它现在学会了“识别身份”。如果发现某个 `-` 或 `*` 处于一元运算符的位置（比如在句首），它会**跳过**。
   - **结果**：它只返回真正的二元分割点。
3. **单目运算符剥壳**：如果第 2 步没找到分割点，`eval` 才检查开头是不是 `-` 或 `*`。
   - 如果是，剥掉符号，递归处理 `eval(p + 1, q)`。
4. **原子项识别**：如果以上都不是，那它就是一个纯粹的数字或寄存器。

### 3.5 纯粹的获取主运算符函数

贴一下增加了精确识别二元运算符`find_main_operator`的实现：

```C
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
```

