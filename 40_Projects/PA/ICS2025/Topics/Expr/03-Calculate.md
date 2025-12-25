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

推荐框架（伪代码级）

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

  /* NOT_WRAP → 普通表达式处理 */
  int op = find_main_operator(p, q);

  // 二元运算
  if (op != -1) {
    word_t val1, val2;
    EvalErrType err;
  
    err = eval(p, op - 1, &val1);
    if (err != EVAL_OK) {
      return err;
    }
  
    err = eval(op + 1, q, &val2);
    if (err != EVAL_OK) {
      return err;
    }
  
    if (tokens[op].type == '/' && val2 == 0) {
      return EVAL_ERR_DIV_ZERO;
    }
  
    *res = apply_binary_operator(op, val1, val2);
    return EVAL_OK;
  } else {
    // 一元运算 - 仅支持负号
    if (tokens[p].type == '-') {
      word_t val;
      EvalErrType err = eval(p + 1, q, &val);
      if (err != EVAL_OK) {
        return err;
      }
      *res = -val;
      return EVAL_OK;
    } else {
      return EVAL_ERR_BAD_EXPRESSION;
    }
  }
}
```

这份 `eval()` 结构是**非常成熟的工程级写法**：

- ✔ **返回错误码而不是 assert**
- ✔ **错误一层层向上传播**
- ✔ `check_parentheses` 只负责**结构判定**，不混业务
- ✔ `eval` 是一个**纯计算 + 错误传播函数**

## 2. 测试 `eval()`正确性

### 2.1 测试文件 :card_index:

课程提供了表达式生成器的框架代码，最终生成一个包含有 10000 条测试数据的测试文件。其中每行为一个测试用例, 其格式为：

```tex
结果 表达式
```

### 2.2 NEMU 读入测试文件

#### (1) make run 做了什么

在`nemu/`目录下执行指令：

```bash
make run
```

最终运行的是：

```bash
riscv32-nemu-interpreter --log=.../nemu-log.txt
```

这些参数由 `native.mk` 中的 `Makefile` 规则拼接生成，并在启动时由`monitor`模块的：

```c
parse_args(argc, argv)
```

解析。

#### (2) 如何读入测试文件

根据上面读入`log`文件的方法作为指导，我们可以较为简单地读入测试文件了。

- 补充`parse_args()`方法：加入测试文件路径的解析
- 新增解析测试文件的方法：暂时放在`expr.c`中，这就引出另一个问题：NEMU 如何组织头文件？
- 在`native.mk`中加入测试文件输入：修改`ARGS`参数

### 2.3  :scissors:最终截断 VS 中间截断

在测试的时候，发现测试案例和真正的表达式求值结果不一致：

```bash
  805079596 = (1906384114 *  486346479 / 5  +1285362082 /  5  - 188528831*  3433859010)
```

这是 C 语言在最终阶段截断了溢出部分代码

```TEX
全部用无限精度整数
→ 最后一次性得到结果
→ 再截断
```

而我们通过`eval()`计算的最终结果，不是“算完再截断”， 而是“每一步都在固定宽度里算”：

```tex
step1: 1906384114 * 486346479  → 截断
step2: 上一步结果 / 5        → 截断
step3: 1285362082 / 5         → 截断
step4: 188528831 * 3433859010 → 截断
step5: 加减                   → 截断
```

👉 **每一步都在 mod 2^32 的环上转**。

> 我要的是：
>  👉 数学上先用“无限精度 / 足够大精度”把整个表达式算完
>  👉 最后一步，再截断到 32 位

想“最后再截断”，就必须让 `eval()` 的中间结果“永远不溢出”。在 C 里，这只能靠更大的类型或大整数。

#### (1) 计算类型 & 返回类型分离

如果你不想 `word_t` 一直是 64 位（比如将来要支持不同模式）：

1️⃣ 定义计算类型

```C
typedef uint64_t eval_word_t;   // 内部计算
```

2️⃣ 改 `eval` 签名（更清晰）

```C
bool eval(int p, int q, eval_word_t *res, EvalError *err);
```

3️⃣ `expr` 做最终截断

```C
eval_word_t result;
eval(0, nr_token - 1, &result, &err)
return (word_t)result;
```

#### (2) 丢掉的 1:question:

已经把“中间溢出”的问题彻底解决了。我们再测试下计算结果居然是`805079595`，跟目标结果差了个`1`。

我们把“被截断的小数部分”单独拿出来看：

**第一项的损失**：

```tex
… / 5 = xxx.2  → 丢了 0.2
```

**第二项的损失**：

```tex
… / 5 = xxx.4  → 丢了 0.4
```

合计：

```tex
0.2 + 0.4 = 0.6
```

👉 这些 **在整数除法时全部被丢掉了**

而“期望的结果”本质上是：

> **先把这些小数累积起来，最后再整体取整**

所以现在真相非常清晰了：

✅ **现在的实现语义是**：

> 无限精度整数
>
> - 每个 `/` 都是整数除法
> - 最终统一截断到 32 位**

→ **结果必然是** `805079595`

------

❌ 而你心里想要的是：

> 无限精度实数 / 有理数
>
> - 所有运算做完
> - 最后统一取整 / 截断**

→ **结果才会是 `805079596`**

所以我们修改`eval_word_t`的数据类型为：

```C
typedef long double eval_word_t; // 内部计算使用更高精度的类型 
```

这样就可以得到与目标结果一致的结果了。
