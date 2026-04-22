#  📘 AxisViewModel 业务语义约束文档（v1.0）

------

# 一、定义（核心定位）

```text
AxisViewModel =
    单轴控制系统的“唯一交互入口”
    + 状态投影器
    + 系统驱动器（tick）
```

------

## 🎯 目标

```text
✔ UI 只能通过 ViewModel 操作系统
✔ UI 看到的状态必须来自真实反馈
✔ 不引入新的业务逻辑
✔ 不破坏 Domain / Orchestrator 语义
```

------

# 二、职责边界（必须严格遵守）

------

## ✅ ViewModel 负责：

```text
✔ 接收 UI 输入（按钮/输入框）
✔ 调用 UseCase / Orchestrator
✔ 驱动系统 tick 循环
✔ 将 Axis 状态映射为 UI 可读数据
✔ 控制信号节流（避免UI爆炸刷新）
```

------

## ❌ ViewModel 禁止：

```text
❌ 不允许直接修改 Axis 状态
❌ 不允许实现业务规则（如运动判定）
❌ 不允许绕过 Orchestrator 调用 Domain
❌ 不允许缓存“真实状态”（必须来自 Axis）
```

------

# 三、状态语义约束（State Projection）

------

## 🟢 1. 状态来源

```text
所有状态必须来自：

Axis.applyFeedback(...) 的结果
```

------

## ❌ 禁止：

```text
ViewModel 内部维护 state / pos / velocity
```

------

------

## 🟢 2. 状态一致性

```text
UI 显示状态 == Axis 当前状态
```

------

### 约束：

```text
✔ 不允许 UI 显示“Moving”，但 Axis 实际是 Idle
✔ 不允许 UI 显示“Done”，但 Axis 未完成
```

------

------

## 🟡 3. 状态更新频率

```text
状态变化驱动信号（NOTIFY）
```

------

### ❗约束：

```text
✔ 只有值变化才 emit
❌ 每 tick 都 emit（禁止）
```

------

------

# 四、控制语义约束（Control Semantics）

------

# 🟢 1. Jog 行为

------

## 定义

```text
jogPositivePressed → StartJog(Positive)
jogPositiveReleased → StopJog(Positive)
```

------

## 约束

```text
✔ 按下 → 必须发起 Jog
✔ 松开 → 必须发起 StopJog
✔ 必须通过 JogOrchestrator
```

------

## ❌ 禁止

```text
❌ 按下后无反应
❌ 松开后继续运动
❌ 直接调用 Axis.jog（绕过 orchestrator）
```

------

------

# 🟢 2. Move 行为

------

## 定义

```text
moveAbsolute(pos)
moveRelative(delta)
```

------

## 约束

```text
✔ 必须通过 Orchestrator 执行
✔ 必须形成完整闭环（Enable → Move → Disable）
```

------

## ❌ 禁止

```text
❌ 直接调用 Axis.moveAbsolute
❌ 未完成就允许再次调用（除非策略允许）
```

------

------

# 🟢 3. Stop 行为

------

## 定义

```text
stop() → StopAxisUseCase
```

------

## 约束

```text
✔ 任意状态可调用
✔ 必须最终导致运动停止
✔ 必须通过 UseCase（非直接Axis）
```

------

------

# 🟡 4. Enable / Disable

------

## 约束

```text
✔ enable() → Axis进入Enabled
✔ disable() → Axis进入Disabled
✔ 不允许跳过流程
```

------

------

# 五、tick 语义（系统驱动核心）

------

## 🧠 定义

```text
tick() = 系统唯一推进入口
```

------

## 必须执行流程（严格顺序）

```text
1. Orchestrator.step()
2. Axis.pendingIntent → Driver.send()
3. Driver.poll() / FakePLC.step()
4. 获取 Feedback
5. Axis.applyFeedback()
6. 更新 UI 状态
```

------

------

## ❗约束

```text
✔ 所有行为必须通过 tick 推进
❌ 不允许“瞬间完成”
```

------

------

# 六、命令生命周期约束（非常关键）

------

## 🟢 1. Intent 必须被消费

```text
Axis.pendingIntent → 最终必须变为空
```

------

------

## 🟢 2. ViewModel 不持有命令状态

```text
命令生命周期完全由 Axis 控制
```

------

------

## ❌ 禁止

```text
ViewModel 维护“当前命令状态”
```

------

------

# 七、异常与错误语义

------

## 🟢 1. 错误来源

```text
Axis / Orchestrator → ViewModel
```

------

------

## 🟢 2. UI展示

```text
✔ hasError == true
✔ errorMessage 可读
```

------

------

## ❗约束

```text
✔ 错误必须可见
✔ 错误不得被吞掉
```

------

------

# 八、系统级约束（Integration）

------

## 🔥 约束1：ViewModel 不改变系统语义

```text
UI层加入后：

系统行为必须与原SystemIntegration一致
```

------

------

## 🔥 约束2：必须可回归

```text
✔ 所有原有测试通过
✔ 行为无变化
```

------

------

## 🔥 约束3：可重复性

```text
相同输入 → 相同结果
```

------

------

# 九、可直接转 TDD 的测试语义

------

# 🧪 状态类

```text
StateShouldReflectAxisState
PositionShouldUpdateFromFeedback
```

------

------

# 🧪 Jog 类

```text
JogPositiveShouldStartWhenPressed
JogPositiveShouldStopWhenReleased
JogShouldUseOrchestrator
```

------

------

# 🧪 Move 类

```text
MoveAbsoluteShouldTriggerOrchestrator
MoveShouldCompleteThroughTickLoop
```

------

------

# 🧪 Stop 类

```text
StopShouldHaltMotionImmediately
```

------

------

# 🧪 tick 类

```text
TickShouldDriveSystemForward
TickShouldSendPendingIntentToDriver
TickShouldUpdateAxisFromFeedback
```

------

------

# 🧪 异常类

```text
ErrorShouldBeExposedToUI
SystemShouldNotHideFailure
```

------

------

# 十、验收标准（Definition of Done）

------

## ✅ 必须满足：

```text
✔ 所有 UI 操作通过 ViewModel
✔ UI 状态完全来自 Axis
✔ tick 驱动完整系统闭环
✔ 所有原有测试通过
✔ 新增 ViewModel 测试通过
✔ 无状态不同步问题
```

------

------

# 🧠 十一、最核心一句话（必须记住）

------

```text
AxisViewModel 不是“逻辑层”，

它是：

👉 “系统的控制面板接口 + 状态镜像”
```

------

------

# 🔥 最后给你一个关键建议

------

你接下来写 TDD 时：

```text
每一条测试都问自己：

👉 “这是不是 UI 用户能观察到的行为？”
```

------

如果不是：

👉 不属于 ViewModel 测试

------

