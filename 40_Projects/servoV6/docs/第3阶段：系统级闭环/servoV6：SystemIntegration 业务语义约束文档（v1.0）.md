------

# 📘 servoV6：SystemIntegration 业务语义约束文档（v1.0）

------

# 一、定义（Definition）

```text
SystemIntegrationTest 是对以下链路的端到端验证：

Axis（Domain）
→ UseCase（Application）
→ Driver（执行）
→ FakePLC（物理模拟）
→ Feedback（回流）
→ Axis.applyFeedback（状态收敛）
```

------

## 🎯 目标

```text
验证系统是否具备：

✔ 行为正确性（Behavior Correctness）
✔ 闭环完整性（Closed Loop Integrity）
✔ 可观测性（Observability）
✔ 可重复性（Determinism）
```

------

# 二、系统边界（System Boundary）

------

## ✅ 测试包含：

```text
✔ Axis（完整）
✔ UseCase（axis层）
✔ Orchestrator（policy层）
✔ FakeAxisDriver
✔ FakePLC（含时间推进）
✔ AxisSyncService（或等价逻辑）
```

------

## ❌ 测试不包含：

```text
❌ UI
❌ Qt
❌ 真实PLC
❌ 网络通信
```

------

👉 原则：

```text
SystemIntegration = “纯软件闭环物理模拟系统”
```

------

# 三、核心语义模型（Core Model）

------

## 🔁 系统运行本质

```text
系统以“离散时间步（Tick）”运行：

每个 Tick：

1. Orchestrator 决策
2. UseCase 触发命令
3. Driver 发送命令
4. FakePLC 执行（step）
5. 生成 Feedback
6. Axis.applyFeedback
```

------

## 🧠 一句话总结：

```text
系统 = 一个“可观测离散时间物理仿真系统”
```

------

# 四、时间模型（Time Model）

------

## ⏱ Tick 定义

```text
Tick = 系统最小推进单位（例如 10ms）
```

------

## ❗约束

```text
✔ 所有状态变化必须通过 Tick 推进
❌ 禁止瞬间完成（无物理过程）
```

------

## 示例：

```text
Move：
    pos(t) → pos(t+1) → ... → target

Jog：
    持续变化，直到 stop
```

------

# 五、可观测量（Observables）

------

## 5.1 Axis（Domain）

```text
state
absPos
relPos
jogVelocity
moveVelocity
pendingIntent
```

------

## 5.2 Orchestrator

```text
step（Idle / EnsuringEnabled / ...）
motionObserved（Move场景）
direction（Jog场景）
```

------

## 5.3 FakePLC（物理）

```text
position
velocity
jogPositiveActive
jogNegativeActive
enabled
```

------

👉 原则：

```text
所有断言必须基于“可观测量”
```

------

# 六、行为分类（必须区分）

------

## 🟢 1. 事务型行为（Move）

```text
✔ 有开始
✔ 有结束
✔ 必须完成或失败
✔ 必须验证“发生过运动”
```

------

👉 已定义规范：

------

## 🔵 2. 持续型行为（Jog）

```text
✔ 无完成概念
✔ 由输入控制生命周期
✔ 必须响应 stop
```

------

👉 已定义规范：

------

# 七、系统级约束（System-Level Invariants）

------

## 🔥 约束1：命令必须经过完整链路

```text
Axis → Driver → PLC → Feedback → Axis
```

------

❌ 禁止：

```text
Axis 直接改变状态
```

------

## 🔥 约束2：状态必须来自反馈

```text
Axis.state = feedback.state
```

------

❌ 禁止：

```text
UseCase / Orchestrator 直接改 state
```

------

## 🔥 约束3：命令必须被消费

```text
pendingIntent → 最终必须消失
```

------

## 🔥 约束4：行为必须“可观测”

```text
Move → 必须看到位置变化
Jog → 必须看到持续变化
```

------

## 🔥 约束5：系统必须可重复

```text
同样输入 → 同样结果
```

------

# 八、标准测试场景（必须覆盖）

------

# 🧪 场景1：绝对定位（成功路径）

------

## 🎯 目标

```text
验证完整事务闭环
```

------

## 流程

```text
1. 初始：Disabled
2. Start Move（Orchestrator）
3. 自动 Enable
4. 发 Move
5. 位置逐步变化
6. 到达 target
7. Disable
8. Done
```

------

## ✅ 必须断言

```text
✔ motionObserved == true
✔ absPos ≈ target
✔ state == Disabled
✔ pendingIntent == none
✔ step == Done
```

------

# 🧪 场景2：相对定位

------

```text
目标 = startPos + offset
```

------

## 额外断言

```text
✔ 最终位置正确计算
```

------

# 🧪 场景3：Move 失败（异常路径）

------

## 条件

```text
Move 被拒绝 / 未发生运动 / 中途停止
```

------

## 必须断言

```text
✔ step == Error
✔ 不允许进入 Done
```

------

# 🧪 场景4：点动（正常流程）

------

## 流程

```text
StartJog
→ Enable
→ Jogging（位置持续变化）
→ StopJog
→ 停止
→ Disable（可选）
→ Done
```

------

## 必须断言

```text
✔ 位置持续变化
✔ Stop后位置稳定
✔ state回到Idle/Disabled
```

------

# 🧪 场景5：点动中断（异常）

------

## 条件

```text
限位触发 / Error
```

------

## 必须断言

```text
✔ Jog停止
✔ step进入Error 或 Stop流程
```

------

# 🧪 场景6：Stop 行为

------

## 目标

```text
验证 Stop 必须真正“停住”
```

------

## 必须断言

```text
✔ velocity → 0
✔ position 不再变化
```

------

# 九、FakePLC 行为约束（非常关键）

------

## 🔧 Move 行为

```text
位置逐步逼近 target
速度有限
```

------

## 🔧 Jog 行为

```text
enable → 持续运动
disable → 减速停止
```

------

## 🔧 限位

```text
触发 → 强制停止
```

------

## ❗禁止：

```text
瞬间到位
瞬间停止
```

------

# 十、终止条件（Test Completion）

------

## 每个测试必须有：

```text
✔ 明确终止条件（Done 或 Error）
✔ 最大 Tick 限制（防死循环）
```

------

## 示例：

```cpp
for (int i = 0; i < MAX_TICKS; ++i)
```

------

# 十一、失败判定（Failure Criteria）

------

## ❌ 失败条件

```text
1. 超时（未完成）
2. 状态错误
3. 未发生运动
4. 位置不正确
5. intent未清理
```

------

# 十二、一句话总结（最重要）

------

```text
SystemIntegrationTest 的本质是：

👉 验证“一个物理行为是否真实发生并被系统正确感知”
```

------

# 🔥 十三、你接下来该做什么（直接执行）

------

## 第一步（今天就做）

```text
写一个：

ShouldCompleteAbsoluteMoveEndToEnd
```

------

## 第二步

```text
写：

ShouldJogStartAndStopCorrectly
```

------

## 第三步

```text
写：

ShouldFailWhenMoveInterrupted
```

------

------

# 🧠 最后一刀（核心认知）

------

你现在的系统已经不是：

```text
写代码
```

而是：

```text
构建“可验证的物理系统模型”
```

------

------

如果你愿意，下一步我可以帮你：

👉 **把这个文档 → 直接展开成完整 GTest 测试代码（含 FakePLC 行为实现）**

这一步会让你开发速度直接翻倍。