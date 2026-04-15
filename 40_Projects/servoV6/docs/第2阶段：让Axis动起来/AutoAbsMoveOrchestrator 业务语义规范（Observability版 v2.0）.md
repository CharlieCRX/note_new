------

# 📘 servoV6：AutoAbsMoveOrchestrator 业务语义规范（Observability版 v2.0）

------

# 一、定义（Definition）

```text
AutoAbsMoveOrchestrator 是一个“基于可观测性驱动的闭环任务编排器”，

负责将“绝对位置移动”这一业务意图，
转化为一组有序命令（Enable → Move → Disable），

并通过 Axis 的“状态 + 连续信号（位置）”共同判断任务进度与完成性。
```

------

# 二、核心思想（Core Concept）

------

## 🔥 从“状态驱动”升级为“可观测驱动”

```text
旧模型：
    仅依赖 AxisState（离散）

新模型：
    AxisState（离散） + Position（连续） = 可观测系统
```

------

## 🚨 关键结论

```text
“是否发生运动” ≠ “是否进入 Moving 状态”

而是：

👉 是否观测到有效位移（Position Delta）
```

------

# 三、可观测量定义（Observables）

Orchestrator 只允许依赖以下输入：

------

## 3.1 离散状态（Discrete State）

```text
AxisState:
    Disabled
    Idle
    MovingAbsolute
    Error
```

------

## 3.2 连续信号（Continuous Signals）

```text
absPos      // 当前绝对位置
velocity    // （可选）
```

------

## 3.3 内部观测变量（Orchestrator维护）

```cpp
double startPos;         // 启动时位置
double lastPos;          // 上一周期位置
bool   motionObserved;   // 是否已观测到运动
```

------

## 3.4 位移判定（核心）

```text
motionObserved = true  当：

|absPos - startPos| > epsilon
```

------

## ⚠️ epsilon 定义

```text
epsilon = 编码器分辨率 * 系数（建议 2~5）
```

👉 防止：

```text
噪声 / 抖动 / 量化误差
```

------

# 四、任务语义（Task Semantics）

------

## 🎯 绝对定位任务定义

```text
一个“必须完成或失败”的闭环事务（Transaction）：

✔ 有明确目标（target）
✔ 必须经历“运动发生”
✔ 必须验证“位置收敛”
```

------

## 🚨 核心约束

```text
未观测到运动 → 不允许判定完成
```

------

# 五、状态机语义（Step Semantics）

------

## 🧩 Step: Initial

```text
未启动，不产生任何行为
```

------

## 🧩 Step: EnsuringEnabled

```text
目标：
    确保 Axis 进入 Idle（可执行状态）

行为：
    Disabled → Enable(true)

推进：
    Axis == Idle → IssuingMove
```

------

## 🧩 Step: IssuingMove

```text
目标：
    发起移动请求（仅一次）

行为：
    调用 moveAbsolute(target)

分支：
    ✔ 成功 → 记录 startPos，进入 WaitingMotionStart
    ❌ 失败 → Enable(false) + Error
```

------

## 🧩 Step: WaitingMotionStart（可观测驱动）

------

### 🎯 目标

```text
确认“运动已经真实发生”
```

------

### ✅ 判定条件（任一成立）

```text
1. AxisState == MovingAbsolute
2. |absPos - startPos| > epsilon
```

------

### 🚨 关键约束

```text
✔ 不允许仅依赖 AxisState
✔ 必须允许“未采样到 Moving”的情况
```

------

### 推进

```text
满足条件 → motionObserved = true → WaitingMotionFinish
```

------

## 🧩 Step: WaitingMotionFinish

------

### 🎯 目标

```text
确认“运动完成且收敛”
```

------

### ✅ 完成条件（必须全部满足）

```text
1. motionObserved == true
2. AxisState == Idle
3. |absPos - target| <= tolerance
4. noPendingCommand
```

------

### ❌ 异常条件

```text
AxisState == Idle 且：

|absPos - target| > tolerance
```

👉 说明：

```text
✔ 运动被中断
✔ 未到达目标
✔ 控制失败
```

------

### 推进

```text
✔ 完成 → IssuingDisable
❌ 异常 → Error
```

------

## 🧩 Step: IssuingDisable

```text
发送 Enable(false)

等待 Axis == Disabled → Done
```

------

## 🧩 Step: Done

```text
任务成功结束（不可再推进）
```

------

## 🧩 Step: Error

```text
任务失败终止（不可再推进）
```

------

# 六、关键语义规则（Critical Rules）

------

## 🔥 规则1：运动必须被“观测”

```text
没有位置变化 → 就没有发生运动
```

------

## 🔥 规则2：Idle 不具有语义

```text
Idle 可能表示：

✔ 未开始
✔ 已完成
✔ 异常中断

👉 必须结合“路径 + 位置”判断
```

------

## 🔥 规则3：完成必须具备“路径证明”

```text
Initial
→ IssuingMove
→ WaitingMotionStart（观测到运动）
→ WaitingMotionFinish
→ Done
```

------

## 🔥 规则4：状态不可回推

```text
❌ 不能因为发了 Move，就认为进入 Moving
❌ 不能因为 Idle，就认为完成
```

------

## 🔥 规则5：motionObserved 单向锁定

```text
一旦为 true，不允许回退
```

------

# 七、错误处理语义（Error Handling）

------

## 🚨 触发条件

```text
✔ AxisState == Error
✔ Move 被拒绝
✔ 未到位但回到 Idle
```

------

## 🚨 行为

```text
✔ 停止流程
✔ 可执行 Enable(false)
✔ Step → Error
```

------

# 八、与 Jog 的严格区分（非常关键）

------

## ❌ AutoAbsMoveOrchestrator 不允许处理：

```text
Jogging
持续运动控制
实时输入响应
```

------

## ✅ absMove 特征

```text
✔ 离散任务（一次性）
✔ 有完成判定
✔ 强一致性
```

------

## ❌ Jog 特征

```text
✔ 持续输入驱动
✔ 无完成概念
✔ 不依赖位置收敛
```

------

# 九、一句话总结

```text
AutoAbsMoveOrchestrator 的本质是：

👉 用“状态 + 位置变化”共同证明
   一次物理运动“确实发生并成功完成”
```

------

# 🔥 十、你这一步的真正价值

你已经从：

```text
写状态机
```

升级到了：

```text
设计“可观测系统（Observable System）”
```

------

# 🚀 下一步建议（非常关键）

如果你继续往前走，我强烈建议你做这个：

👉 把 `Axis` 抽象为：

```text
State + Signals + Timestamp
```

然后你可以做到：

```text
✔ 判断速度（Δpos / Δt）
✔ 判断卡滞
✔ 判断抖动
✔ 做更高级控制策略
```

------

如果你愿意，我们下一步可以把：

👉 **这个语义 → 直接落成 C++ 实现（无歧义版本）**

那会是你这个项目的“分水岭级提升”。