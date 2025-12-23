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

当前`eval()`结构中，发现非法表达式的时候使用`assert(0)`终止程序。`assert(0)` 的语义是：

> **“这是程序员不可能犯的错误”**

但是现在`eval()`需要面对的表达式来源可能是：

- 用户输入
- 配置表达式
- 运行期字符串

👉 **这是“外部不可信输入”**。

:snowboarder: **外部输入永远不能用 assert 处理。**

所以原来（教学阶段）：

> `eval()`
>  👉 *假设输入合法，否则程序终止*

现在（工程阶段）：

> `eval()`
>  👉 *在不可信输入下，尝试求值；失败则返回明确错误*

这一步，决定了我们**必须引入错误码**。

