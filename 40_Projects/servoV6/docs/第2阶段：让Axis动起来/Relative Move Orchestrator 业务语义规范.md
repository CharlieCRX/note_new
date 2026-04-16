------

# 📘 Relative Move Orchestrator（策略层语义约束）

------

# 🧠 1. 设计目标（与绝对定位的本质区别）

```text
Relative Move ≠ “去某个绝对位置”

而是：

👉 “从当前时刻的物理位置出发，移动一个相对位移 Δ”
```

------

## 🎯 核心特性

```text
1. 起点是“执行瞬间”的 currentAbsPos（动态）
2. 目标是“隐式目标”（startPos + Δ）
3. 是一次“闭环位移事务”，而不是位置设定
```

------

# 🧠 2. 整体流程语义（高层）

```text
Initial
 → EnsuringEnabled
 → IssuingMove
 → WaitingMotionStart
 → WaitingMotionFinish
 → Done / Error
```

👉 与 Absolute Move **流程结构一致**，但**语义不同**

------

# 🧠 3. Step 级语义定义

------

## 🔹 Step: EnsuringEnabled

### 🎯 目标

```text
确保 Axis 进入 Idle（可接受 Move 的状态）
```

------

### ✅ 行为约束

```text
1. AxisState == Error
   → 进入 Error

2. AxisState == Disabled
   → 发 Enable(true)

3. AxisState == Idle
   → 进入 IssuingMove
```

------

### ❗注意

```text
RelMove 不允许在非 Idle 时计算 Δ
（否则起点不确定）
```

------

## 🔹 Step: IssuingMove

------

### 🎯 目标

```text
发起一次“相对位移命令”
```

------

### ✅ 行为约束

```text
1. 必须调用 moveRelative(distance)

2. MoveCommand.startAbs = 当前 absolute position（Axis 内部已处理）

3. Move 只能发送一次（幂等）

4. 若被拒绝：
   → Enable(false)
   → 进入 Error

5. 成功：
   → 记录 startPos
   → 进入 WaitingMotionStart
```

------

### ❗关键语义（区别于 Absolute）

```text
Relative Move 的 target ≠ m_target

而是：

👉 physicalTarget = startPos + distance
```

------

## 🔹 Step: WaitingMotionStart

------

### 🎯 目标

```text
确认“运动已经发生”
```

------

### ✅ 判定条件（任一成立）

```text
1. AxisState == MovingRelative

2. abs(currentPos - startPos) > epsilon
```

------

### ❗关键语义

```text
不能依赖 MovingRelative

必须使用“位置变化”作为兜底
```

------

### ❌ 未发生

```text
→ 保持当前状态（等待 or timeout）
```

------

## 🔹 Step: WaitingMotionFinish

------

### 🎯 目标

```text
确认“位移已完成且收敛”
```

------

### ✅ 完成条件（必须全部满足）

```text
1. motionObserved == true

2. AxisState == Idle

3. |currentPos - (startPos + distance)| <= tolerance
```

------

### ❗关键语义

```text
Relative Move 的完成判定：

👉 不是 target
👉 而是 startPos + distance
```

------

### ❌ 异常条件

```text
Idle 且未到位
→ Error（或未来扩展 retry）
```

------

## 🔹 Step: Done

```text
→ 表示本次“位移事务”成功完成
```

------

## 🔹 Step: Error

```text
→ 任意阶段异常终止
→ 不保证位置正确
```

------

# 🧠 4. 核心不变量（非常重要）

------

## ✅ 不变量1：起点不可变

```text
startPos 一旦记录，不可修改
```

------

## ✅ 不变量2：目标是隐式的

```text
targetPos = startPos + distance
```

------

## ✅ 不变量3：必须观测到运动

```text
未 motionObserved → 不允许完成
```

------

## ✅ 不变量4：Idle ≠ 完成

```text
必须同时满足：

✔ Idle
✔ 收敛
✔ 运动发生过
```

------

## ✅ 不变量5：RelMove 是“事务”

```text
要么成功完成位移
要么进入 Error

不存在“部分成功”
```

------

# 🧠 5. 与 Absolute Move 的关键区别

------

## ⚠️ 差异1：目标来源

```text
Absolute: target 直接给定
Relative: target = startPos + Δ（运行时计算）
```

------

## ⚠️ 差异2：起点依赖

```text
Absolute: 与当前点无关
Relative: 强依赖 startPos
```

------

## ⚠️ 差异3：误差传播

```text
Relative Move：

误差会累积（多次相对运动）

Absolute Move：

不会累积（每次重新锚定）
```

------

## ⚠️ 差异4：失败风险

```text
Relative Move 更容易：

✔ 累积误差
✔ 超限
✔ 偏移漂移
```

------

# 🧠 6. Timeout 语义（必须支持）

------

## ✅ WaitingMotionStart

```text
长时间未观测到运动 → Error
```

------

## ✅ WaitingMotionFinish

```text
长时间未收敛 → Error
```

------

## ❗注意

```text
RelMove 比 AbsMove 更需要 timeout
（因为误差可能导致永远无法到位）
```

------

# 🧠 7. 错误处理语义

------

## ✅ 全局规则

```text
AxisState == Error → 立即进入 Error
```

------

## ✅ Move Rejection

```text
→ Disable
→ Error
```

------

## ✅ 偏差过大

```text
Idle + 未到位 → Error
```

------

# 🎯 最后一刀总结（可以作为文档收尾）

```text
Relative Move Orchestrator 的本质是：

👉 管理一个“基于当前物理位置的位移事务”

它不关心“去哪里”，

而关心：

👉 “从哪里出发，移动了多少，并且确实完成了这次位移”
```

------

# 🚀 如果你下一步继续（我强烈建议）

可以继续做两件非常有价值的事情：

------

## 1️⃣ RelMove 的 TDD（会比 Abs 更复杂）

我可以帮你拆一套测试：

```text
✔ 连续多次 relative move（误差验证）
✔ 小位移
✔ 方向切换
✔ 超限场景
```

------

## 2️⃣ Abs + Rel 统一抽象（高级架构）

```text
MoveOrchestrator<TargetPolicy>
```

👉 让你整个系统：

```text
不用复制逻辑
只替换“目标计算策略”
```

------

如果你愿意，我们下一步可以直接进入：

👉 **RelMove 的 TDD设计（会很有挑战性，也很有价值）**