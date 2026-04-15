很好，这一步你已经在做**真正的“语义分离”设计了** 👍
你明确区分了：

```text
绝对定位（absMove） = 闭环任务 ✔
JOG 点动           = 持续控制流 ❌（完全不同范式）
```

------

# 🧠 一、先把本质说清楚（非常重要）

## ❌ 之前的问题

之前的状态机其实“隐含支持了多种运动”，导致：

```text
MovingAbsolute / MovingRelative / Jogging 混在一起
```

👉 这在语义上是错误的

------

## ✅ 正确认知（你这一步已经对了）

### 🎯 absMove（绝对定位）

```text
✔ 一次性任务（One-shot Task）
✔ 有明确目标（target）
✔ 必须完成或失败
✔ 是闭环（Closed-loop）
```

------

### 🎮 Jog（点动）

```text
✔ 持续控制（Streaming / Control Loop）
✔ 没有“完成”概念
✔ 输入驱动（按钮 / 摇杆）
✔ 是开环/半闭环
```

------

# 🚨 结论（必须写进规范）

```text
absMove 和 Jog 必须是两个完全独立的状态机
```

👉 Orchestrator（AutoMove）：

```text
❌ 不允许处理 Jog
✔ 只负责 absMove
```

------

# 📘 absMove 状态机（纯净版）

我们现在**只建模绝对定位**

------

# 🎯 状态机目标（重新定义）

```text
从“当前状态” → “移动到 target” → “确认完成” → “安全退出”
```

------

# 🧩 状态定义（精简且无歧义）

```cpp
enum class Step {
    Initial,              // 未启动
    EnsuringEnabled,      // 确保可执行移动（Axis → Idle）
    IssuingMove,          // 发起绝对定位（一次）
    WaitingMotionStart,   // 等待“真正开始运动”
    WaitingMotionFinish,  // 等待“从运动回到Idle且到位”
    IssuingDisable,       // 掉电（策略决定）
    Done,
    Error
};
```

------

# ⚠️ 关键变化（对比你之前）

```text
❌ 删除所有 Jog / Relative 相关判断
❌ Moving 集合只允许：MovingAbsolute
```

👉 非常关键：

```text
absMove 状态机只认 MovingAbsolute
```

------

# 📊 三、absMove 专用状态转移表（最终版）

------

## 🧩 Step: Initial

| Condition     | Action      | Next            |
| ------------- | ----------- | --------------- |
| start(target) | 保存 target | EnsuringEnabled |

------

## 🧩 Step: EnsuringEnabled

| Condition               | Action       | Next                |
| ----------------------- | ------------ | ------------------- |
| Axis = Error            | 无           | Error               |
| Axis = Disabled         | Enable(true) | 保持                |
| Axis = Idle             | 无           | IssuingMove         |
| Axis = MovingAbsolute ❗ | 无           | WaitingMotionFinish |

------

### ⚠️ 解释一个关键点

```text
如果一开始就发现已经在 MovingAbsolute
→ 说明外部已经发过 Move
→ 直接接管，进入等待完成
```

👉 这是“工业容错能力”

------

## 🧩 Step: IssuingMove

| Condition                 | Action        | Next               |
| ------------------------- | ------------- | ------------------ |
| Axis = Error              | Enable(false) | Error              |
| moveAbsolute(target) 成功 | MoveCommand   | WaitingMotionStart |
| moveAbsolute(target) 失败 | Enable(false) | Error              |

------

## 🧩 Step: WaitingMotionStart（关键防抖）

| Condition             | Action        | Next                |
| --------------------- | ------------- | ------------------- |
| Axis = Error          | Enable(false) | Error               |
| Axis = MovingAbsolute | 无            | WaitingMotionFinish |
| Axis = Idle           | 无            | 保持                |

------

### 🔥 这一层是核心价值

防止：

```text
Move → PLC延迟 → 仍然 Idle → 被误判完成 ❌
```

------

## 🧩 Step: WaitingMotionFinish

| Condition                                | Action                  | Next           |
| ---------------------------------------- | ----------------------- | -------------- |
| Axis = Error                             | Enable(false)           | Error          |
| Axis = MovingAbsolute                    | 无                      | 保持           |
| Axis = Idle AND inPosition AND noPending | Enable(false)           | IssuingDisable |
| Axis = Idle BUT not inPosition ❗         | Error处理（或重试策略） | Error          |

------

### ⚠️ 新增一个关键约束

```text
Idle 但未到位 = 异常
```

👉 说明：

```text
✔ 被中断
✔ 被限位打断
✔ 控制失败
```

------

## 🧩 Step: IssuingDisable

| Condition       | Action | Next  |
| --------------- | ------ | ----- |
| Axis = Disabled | 无     | Done  |
| Axis = Error    | 无     | Error |
| 其他            | 无     | 保持  |

------

## 🧩 Step: Done / Error

终止态，不再动作

------

# 🧠 四、absMove 的关键语义升级（这部分很值钱）

------

## 🔥 语义1：完成必须满足“路径正确”

```text
Initial
→ IssuingMove
→ WaitingMotionStart（必须看到 Moving）
→ WaitingMotionFinish
→ Done
```

👉 如果没有经过：

```text
MovingAbsolute
❌ 永远不能完成
```

------

## 🔥 语义2：Idle 分三种（你之前混了）

```text
Idle（未开始）
Idle（刚完成）
Idle（异常中断）
```

👉 必须通过“路径 + 位置”区分

------

## 🔥 语义3：absMove 是“事务”（Transaction）

```text
✔ 要么成功完成
✔ 要么失败退出
✔ 不允许“半完成”
```

------

## 🔥 语义4：Orchestrator 是“接管者”

```text
如果外部已经让轴在 MovingAbsolute：

✔ 不重新发 Move
✔ 直接接管生命周期
```

------

# 🚨 五、你现在系统最大的提升点

你已经完成了这三件关键事：

------

## ✅ 1. 分离了两种控制范式

```text
absMove（任务） vs Jog（控制流）
```

------

## ✅ 2. 建立了“时间语义”

```text
Move ≠ Moving
```

------

## ✅ 3. 引入“路径验证”

```text
必须经过 Moving 才算完成
```

------

# 🎯 最终一句话总结

```text
absMove 不是“发一个定位命令”，

而是：

👉 一个必须经过“启动 → 运动 → 收敛”的受控事务
```

------

# 🚀 下一步（强烈建议）

你现在已经可以做一件非常强的事：

👉 把这个状态机**拆成两个类**：

```text
1. AbsMoveOrchestrator（事务）
2. JogController（实时控制流）
```

这一步会让你整个架构：

```text
从“能用” → “工业级清晰”
```

------

如果你愿意，我下一步可以帮你把：

👉 **Jog 控制模型（完全不同范式）单独建模出来**

那会是你系统的第二个核心能力。