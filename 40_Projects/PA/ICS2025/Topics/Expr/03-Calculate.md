# 表达式求值（三）：递归求值

## 1. `eval()` 的“困境”

递归求值的伪代码如下所示：

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

但是随着开发的不断进行，以及对于工程的考量，需要做出一些结构上的调整以适应富有挑战性的用户输入。

下面就着重分析下当前`eval()`函数面临的困境，以及解决方案。

### 1.1 检测括号逻辑适配

#### (1)  括号的错误码

已经实现的 **检查表达式是否被括号包裹** 的逻辑为：

```C
/* --- 括号检查错误类型 --- */
typedef enum {
  PAREN_ERR_NONE,
  PAREN_ERR_MISMATCH,    // 括号数量或顺序不匹配
  PAREN_ERR_EMPTY,       // 空括号 ()
  PAREN_ERR_NOT_WRAP,    // 不是(expr)类型：(1+2)-(3)或者 )1-2(
  PAREN_ERR_INVALID_RANGE
} ParenErrType;

/**
 * @brief 检查 [start, end] 范围内的 token 是否构成一个合法的 (expr) 结构
 * 逻辑：
 * 1. 首尾必须是 '(' 和 ')'
 * 2. 遍历过程中，左括号必须与右括号抵消
 * 3. 核心判定：在到达最后一个 token 前，括号 balance 不能提前归零（否则说明是 (a+b)+(c+d) 结构）
 */
static bool check_parentheses(int start, int end, ParenErrType *err_type)
```

在`check_parentheses()`函数中，`(4 + 3)) * ((2 - 1)`和`(4 + 3) * (2 - 1)`这两个表达式虽然都返回`false`：

| 表达式                | `check_parentheses` 结果 | 含义                      |
| --------------------- | ------------------------ | ------------------------- |
| `(4 + 3)) * ((2 - 1)` | `PAREN_ERR_MISMATCH`     | **语法错误**              |
| `(4 + 3) * (2 - 1)`   | `PAREN_ERR_NOT_WRAP`     | **合法，但不是 `(expr)`** |

这样加入了`ParenErrType`后的`eval()` 不应该再把 `check_parentheses()` 当作 `bool` 用，而是必须基于 `ParenErrType` 做“结构分流决策”。

#### (2) 分流式框架

1️⃣ 修改点一：接收错误码，而不是 bool

```C
ParenErrType err;
bool is_paren = check_parentheses(p, q, &err);
```

2️⃣ 修改点二：明确区分“错误 vs 结构失败”

```C
  ParenErrType err;
  bool is_paren = check_parentheses(p, q, &err);

  if (is_paren) {
    // 情况 1：是合法的 (expr)
    return eval(p + 1, q - 1);
  }

  // 走到这里：不是 (expr)
  switch (err) {
    case PAREN_ERR_MISMATCH:
    case PAREN_ERR_EMPTY:
    case PAREN_ERR_INVALID_RANGE:
      // 情况 2：结构性错误 → 直接报错
      error(paren_err_msg[err]);
      break;

    case PAREN_ERR_NOT_WRAP:
      // 情况 3：合法表达式，但不是 (expr)
      // 继续正常表达式解析
      break;

    default:
      assert(0);
  }
```

### 1.2  :construction_worker: 程序员错误 vs :keyboard: 外部输入

#### (1) `assert(0)`的拒绝沟通:speak_no_evil:

当前`eval()`结构中，发现非法表达式的时候使用`assert(0)`终止程序。`assert(0)` 的语义是：

> **“这是程序员不可能犯的错误”**:speak_no_evil:

但是现在`eval()`需要面对的表达式来源可能是：

- 用户输入
- 配置表达式
- 运行期字符串

👉 **这是“外部不可信输入”**。

:snowboarder: **外部输入永远不能用 assert 处理。**

所以原来（教学阶段）：

> `eval()`
> 👉 *假设输入合法，否则程序终止*

现在（工程阶段）：

> `eval()`
> 👉 *在不可信输入下，尝试求值；失败则返回明确错误*

这一步，决定了我们**必须引入错误码**。

### 1.3 推荐`eval()`的整体设计方案

#### 1️⃣ 定义表达式求值错误码

```C
typedef enum {
    EVAL_OK = 0,

    EVAL_ERR_INVALID_RANGE,      // p > q
    EVAL_ERR_BAD_EXPRESSION,     // 无法解析为合法表达式
    EVAL_ERR_PAREN_MISMATCH,     // 括号结构错误
    EVAL_ERR_PAREN_EMPTY,        // ()
    EVAL_ERR_DIV_ZERO,           // 除零
    // 后续可扩展
} EvalErrType;
```

👉 **注意：这是“求值层错误码”，不是括号层**

#### 2️⃣ 修改 `eval` 的函数签名（关键一步）

❌ 旧设计（不可恢复错误）

```C
int eval(int p, int q);
```

✅ 新设计（可恢复错误）

```c
EvalErrType eval(int p, int q, word_t *result);
```

含义：

- 返回值：**是否成功 / 失败原因**
- `result`：仅在 `EVAL_OK` 时有效

#### 3️⃣ `eval` 的结构应该变成“错误向上传播”

推荐框架（简略伪代码）

```C
/**
 * @brief 核心抽象函数：递归求值表达式
 *
 * @param p 起始 Token 下标
 * @param q 结束 Token 下标
 * @param res 存储结果的指针
 * @return EvalErrType 错误码
 */
EvalErrType eval(int p, int q, word_t *res) {
  if (p > q) {
    return EVAL_ERR_INVALID_RANGE;
  }

  if (p == q) {
    if (!is_number(p)) {
      return EVAL_ERR_BAD_EXPRESSION;
    }
    *res = token_value(p);
    return EVAL_OK;
  }

  ParenErrType perr;
  bool is_paren = check_parentheses(p, q, &perr);

  if (is_paren) {
    return eval(p + 1, q - 1, res);
  }

  /* 括号相关的“致命结构错误” */
  if (perr == PAREN_ERR_MISMATCH) {
    return EVAL_ERR_PAREN_MISMATCH;
  }
  if (perr == PAREN_ERR_EMPTY) {
    return EVAL_ERR_PAREN_EMPTY;
  }
  if (perr == PAREN_ERR_INVALID_RANGE) {
    return EVAL_ERR_INVALID_RANGE;
  }
  ...
}
```

这份 `eval()` 结构是**较为成熟的工程级写法**：

- ✔ **返回错误码而不是 assert**
- ✔ **错误一层层向上传播**
- ✔ `check_parentheses` 只负责**结构判定**，不混业务
- ✔ `eval` 是一个**纯计算 + 错误传播函数**

## 2.测试`eval()`的正确性

上面我们已经按照 NEMU 对于表达式的要求：

- 无符号运算
- 32 位宽度：`word_t` 保证了所有中间变量和最终结果都被限制在 32 位
- 自然溢出：C 语言标准规定，无符号整数的溢出行为是明确的模 `2^32` 回绕。

那么接下来就要给出生不久的`eval()`上一点小小的强度，让他经过大量的表达式求值，看看是否符合设计目标了。

### 2.1 表达式生成器

NEMU 明确了表达式求值的特性：

- **无符号运算**：所有运算都应视为无符号。

- **32 位宽度**：数据和中间过程均保持 32 位。

- **溢出回绕**：不处理溢出，结果自动模 $2^{32}$。

基于这些特性，参照课程给出的表达式生成器框架：

```C
void gen_rand_expr() {
  switch (choose(3)) {
    case 0: gen_num(); break;
    case 1: gen('('); gen_rand_expr(); gen(')'); break;
    default: gen_rand_expr(); gen_rand_op(); gen_rand_expr(); break;
  }
}
```

于是我便按照这个框架开始实现代码。但在实现的过程中，遇到了真实场景中存在的问题，现在总结一下：

#### (1) 框架代码的便利性与限制性

- 便利性

  - 递归结构自然构造表达式树
  - 对于括号优先级自动处理
  - 框架易于控制复杂度

- 限制性

  - 产生非法表达式：除以 0（divide-by-zero）问题

  - 无法控制 Token（符号 / 数字）数量上限

  -  **难以产生浅层但复杂运算混合的表达式**

    这个递归策略：

    ```C
    default: 
      gen_rand_expr(); gen_rand_op(); gen_rand_expr();
    ```

    是在不断生长树，而不是按 **宽度优先** 或 **用户定义的模板生成**。后果就是生成的更多是随机树不是随机表达式。

#### (2) 除以 0 的非法表达式

每一次生成的表达式，都会被计算结果进行判断，从而遏制了 0 作为除数的情况：

```C
while (1) {
  int temp_r_budget = right_budget;
  r_val = gen_rand_expr(r_c, r_eval, &temp_r_budget);
  if (!(op == '/' && r_val == 0)) break;
}
```

**语义层面解决问题，而不是语法层面**

- 不是“禁止生成 0”
- 而是允许复杂表达式作为除数，只要**最终值 ≠ 0**
- 这非常贴近真实程序的情况（`(a+b-c)` 做除数）

**不污染生成策略**

- 不需要在 `gen_num()` 中排除 0
- 不需要对 `/` 运算做特殊 token 生成规则
- 保持了表达式结构的“随机性”

**与 NEMU 行为严格对齐**

- NEMU 的 `eval()` 是语义求值
- 你在生成阶段就保证语义合法，避免 runtime trap

#### (3) 失控的 Token 数量

使用`budget`机制：

```C
if (*budget < 3) {
  return gen_num(...);
}
```

- 数字：消耗 1 token
- 括号：消耗 2 token
- 运算符：消耗 1 token
- 左右子树预算拆分

优点：

**严格的 token 上界保证**

- 不会生成无限递归
- 表达式长度是 *强约束*，不是概率约束

**预算拆分逻辑是“合法的”**

```C
left_budget = choose(total_remaining - 1) + 1;
right_budget = total_remaining - left_budget;
```

- 左右子树都至少有 1 个 token
- 不会生成空子树或非法结构

**递归停止条件清晰**

- `< 3` 只能生成数字
- 这是从“最小合法表达式”角度出发的，非常严谨

#### (4) C 与 NEMU eval 行为不一致

**双缓冲设计**

```C
char *c_buf;     // 给 C 编译器
char *eval_buf;  // 给 NEMU eval
```

并且：

```c
sprintf(c_buf, "(uint32_t)%uu", num);
sprintf(eval_buf, "%u", num);
```

以及：

```c
((uint32_t)((uint32_t)(%s) %c (uint32_t)(%s)))
```

**优点（方案的“灵魂”）**

1. **强制 C 行为与 NEMU 完全对齐**

   - 明确压制：
     - 整型提升
     - 有符号参与
     - 平台相关 UB
   - 每一层都强转为 `uint32_t`

2. **避免“C 编译器比你聪明”的情况**

   - 编译器不会进行未定义优化
   - 不会利用溢出假设

3. **这是 ICS 课程真正想你理解的点**

   > *“我们不是在比 `eval` 对不对，我们是在比‘模型是否一致’”*

**⚠️ 代价**

- C 表达式**非常丑**
- 可读性几乎为 0
