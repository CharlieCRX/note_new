------

# 📘 servoV6：Orchestrator（策略层语义约束规范 v1.0）

------

# 一、定义（Definition）

```text
Orchestrator（策略层）是一个“任务编排器”，

负责将一个高层业务意图（如：绝对定位）
拆解为一系列有序的命令（Enable → Move → Disable），
并基于 Axis 的反馈状态逐步推进执行。
```

------

# 二、职责（Responsibilities）

Orchestrator **只负责“流程控制”**，其职责严格限定为：

```text
1. 根据当前 Step 和 Axis 状态，决定是否发出命令
2. 管理任务生命周期（start → running → done/error）
3. 处理异步反馈驱动的状态推进
4. 实现业务级约束（自动上电 / 自动掉电 / 失败保护）
```

------

# 三、非职责（Non-Responsibilities）

Orchestrator **必须禁止承担以下职责**：

```text
❌ 不直接修改 Axis 内部状态
❌ 不解释 PLC / 寄存器语义
❌ 不实现具体控制算法（如速度规划）
❌ 不做重试、补偿等底层策略（除非明确设计）
❌ 不假设命令会立即生效（必须接受异步延迟）
```

------

# 四、核心原则（Core Principles）

------

## 4.1 状态驱动原则（State-driven）

```text
Orchestrator 是“被动驱动”的：

✔ 输入：Axis 当前状态（feedback）
✔ 输出：下一步命令（command）
```

👉 不允许：

```text
❌ 主动推进状态（例如：假设 Move 已执行）
```

------

## 4.2 命令与状态分离（Command ≠ State）

```text
命令（Command）：
    系统发出的意图（Enable / Move / Disable）

状态（State）：
    物理系统反馈（Idle / Moving / Error）
```

👉 必须保证：

```text
❌ 不根据“已发命令”推断状态
✔ 只根据“Axis反馈”判断状态
```

------

## 4.3 异步一致性（Asynchronous Consistency）

```text
所有命令均为异步生效：

发送命令 ≠ 状态立即变化
```

👉 系统必须允许：

```text
✔ 状态滞后
✔ PLC执行延迟
✔ 多周期反馈
```

------

## 4.4 单步决策原则（Single-step Decision）

```text
每次 update() 调用：

✔ 最多做一次决策
✔ 最多发送一个命令
```

👉 禁止：

```text
❌ 一个 update 内发送多条命令
❌ 跨多个阶段跳跃执行
```

------

## 4.5 幂等性（Idempotency）

```text
同一阶段的命令必须“只发一次”
```

例如：

```text
✔ MoveCommand 只允许发送一次
✔ Enable(true) 不允许重复发送
```

------

## 4.6 不创造状态（No State Fabrication）

```text
Orchestrator 不得“假设状态已发生”
```

👉 禁止：

```text
❌ 认为 Move 后立即进入 Moving
❌ 未经过 Moving 就判定完成
```

------

# 五、任务生命周期（Lifecycle）

------

## 5.1 状态机定义

```cpp
enum class Step {
    Initial,              // 未启动
    EnsuringEnabled,      // 确保上电
    IssuingMove,          // 发起移动（一次性）
    WaitingMotionStart,   // 等待进入运动状态
    WaitingMotionFinish,  // 等待运动完成
    IssuingDisable,       // 发起掉电
    Done,                 // 成功完成
    Error                 // 失败终止
};
```

------

## 5.2 生命周期流程

```text
Initial
  ↓ start()

EnsuringEnabled
  ↓ (Axis → Idle)

IssuingMove
  ↓

WaitingMotionStart
  ↓ (Axis → Moving)

WaitingMotionFinish
  ↓ (Axis → Idle + 到位)

IssuingDisable
  ↓ (Axis → Disabled)

Done
```

------

# 六、阶段语义约束（Step Semantics）

------

## 6.1 Initial

```text
✔ 未启动
✔ 不发送任何命令
✔ 仅响应 start()
```

------

## 6.2 EnsuringEnabled

```text
目标：
    确保 Axis 进入 Idle（已使能）

行为：
    若 Axis = Disabled → 发送 Enable(true)

推进条件：
    Axis = Idle → 进入 IssuingMove
```

------

## 6.3 IssuingMove

```text
目标：
    发起移动请求（仅一次）

行为：
    调用 MoveUseCase

分支：
    ✔ 成功 → WaitingMotionStart
    ❌ 失败 → IssueDisable + Error
```

------

## 6.4 WaitingMotionStart

```text
目标：
    确认“运动已真正开始”

推进条件：
    Axis ∈ {MovingAbsolute / MovingRelative / Jogging}

约束：
    未进入 Moving 前，不得进入完成判定
```

------

## 6.5 WaitingMotionFinish

```text
目标：
    等待运动完成

完成条件（必须全部满足）：
    ✔ Axis = Idle
    ✔ 曾经进入 Moving（由前一阶段保证）
    ✔ 位置进入目标容差
    ✔ 无 pending command

推进：
    → IssuingDisable
```

------

## 6.6 IssuingDisable

```text
目标：
    执行掉电

行为：
    发送 Enable(false)

推进条件：
    Axis = Disabled → Done
```

------

## 6.7 Done

```text
✔ 任务成功结束
✔ 不再发送任何命令
```

------

## 6.8 Error

```text
✔ 任意阶段可进入
✔ 终止所有流程
✔ 可执行 Fail-safe（如自动 Disable）
```

------

# 七、错误处理语义（Error Handling）

------

## 7.1 异常触发条件

```text
✔ AxisState == Error
✔ Move 被 Domain 拒绝
✔ 超限 / 非法目标
```

------

## 7.2 处理策略

```text
✔ 立即停止流程推进
✔ 禁止继续发送 Move
✔ 可触发 Enable(false)（Fail-safe）
✔ Step → Error
```

------

# 八、输入输出语义（I/O Contract）

------

## 输入（Input）

```text
✔ Axis 当前反馈状态（state + position + flags）
✔ 外部触发（start）
```

------

## 输出（Output）

```text
✔ EnableCommand
✔ MoveCommand
```

------

## ⚠️ 约束

```text
✔ 输出必须通过 UseCase 产生
✔ 不允许直接构造底层命令
```

------

# 九、关键不变量（Invariants）

------

## 🔒 不变量1

```text
未 Enable → 不允许 Move
```

------

## 🔒 不变量2

```text
未进入 Moving → 不可能完成
```

------

## 🔒 不变量3

```text
完成必须来自：
Moving → Idle 的状态跃迁
```

------

## 🔒 不变量4

```text
任意时刻最多存在一个“未完成任务”
```

------

# 十、一句话总结

```text
Orchestrator 不是控制器，

而是：

👉 一个“基于反馈驱动的命令调度器”
```

------

# ✅ 建议落地方式

你可以把这份文档命名为：

```text
docs/auto_move_orchestrator_semantics.md
```

并在代码中强制对齐：

```text
Step命名
测试用例
UseCase职责
```

------

如果你下一步继续深化，我建议我们做这个（价值极高）👇：

👉 把这份语义转成：

```text
状态转移表（State Transition Table）
```

那一步可以做到：

```text
“任何人都写不出错误实现”
```