

> ❗**不要让 Qt 进入你的“可测试核心逻辑”**

否则你后面会遇到：

- gtest 跑不动（QObject / event loop / moc）
- 测试变慢、不可控
- 逻辑和 UI 强耦合（很难维护）

------

# 🎯 一、结论（架构师建议）

------

## ✅ 推荐方案（强烈建议）

```text
AxisViewModel = 两层结构
```

------

### 🟢 1️⃣ Core（纯C++，可测试）

```text
AxisViewModelCore
```

- ❌ 不继承 QObject
- ❌ 不用 signals/slots
- ✅ 可直接用 gtest
- ✅ 只做逻辑（tick / 调用 / 状态投影）

------

### 🟡 2️⃣ Qt Wrapper（适配层）

```text
QtAxisViewModel : public QObject
```

- 只负责：
  - Q_PROPERTY
  - signal
  - QML绑定

------

👉 关系：

```text
UI(QML)
   ↓
QtAxisViewModel（薄）
   ↓
AxisViewModelCore（厚，核心逻辑）
   ↓
UseCase / Orchestrator / Axis
```

------

# 🧭 二、为什么必须这么做

------

## ❌ 如果直接用 QObject 写 VM

你会遇到：

```text
1. gtest 很难测试 signals（需要事件循环）
2. QObject 生命周期复杂
3. 依赖 Qt runtime（CI 变复杂）
4. 测试变“集成测试”，而不是单测
```

------

## ✅ Core 分离后的好处

```text
✔ gtest 直接测逻辑（快、稳定）
✔ Qt层只是“皮”
✔ 后续可以换 UI（甚至不用 Qt）
✔ 更符合 Clean Architecture
```

------

# 🧪 三、TDD 红灯设计思路（核心）

------

## 🎯 原则

```text
测试 AxisViewModelCore
而不是 QtAxisViewModel
```

------

------

# 🧱 四、测试分层（非常重要）

------

# 🥇 第一层：行为测试（最重要）

------

## 1️⃣ Jog（按下/松开）

```cpp
TEST(AxisViewModelCoreTest, JogPositiveShouldStartWhenPressed)
```

------

### 测试逻辑：

```text
Given:
    Axis = Idle
When:
    vm.jogPositivePressed()
Then:
    Axis 产生 JogIntent
```

------

------

```cpp
TEST(AxisViewModelCoreTest, JogPositiveShouldStopWhenReleased)
```

------

```text
Given:
    正在 Jog
When:
    vm.jogPositiveReleased()
Then:
    Axis 产生 StopJogIntent
```

------

------

## 2️⃣ Move

```cpp
TEST(AxisViewModelCoreTest, MoveAbsoluteShouldTriggerOrchestrator)
```

------

```text
Given:
    Idle
When:
    vm.moveAbsolute(100)
Then:
    Orchestrator.start 被调用
```

------

------

## 3️⃣ Stop

```cpp
TEST(AxisViewModelCoreTest, StopShouldTriggerStopUseCase)
```

------

------

# 🥈 第二层：tick 驱动（系统核心）

------

```cpp
TEST(AxisViewModelCoreTest, TickShouldDriveSystemForward)
```

------

### 测试语义：

```text
Given:
    一个 Move 已启动
When:
    多次 tick()
Then:
    位置发生变化
```

------

------

```cpp
TEST(AxisViewModelCoreTest, TickShouldSendIntentToDriver)
```

------

```text
Given:
    Axis 有 pending intent
When:
    tick()
Then:
    Driver.send 被调用
```

------

------

```cpp
TEST(AxisViewModelCoreTest, TickShouldApplyFeedbackToAxis)
```

------

```text
Given:
    Driver 返回 feedback
When:
    tick()
Then:
    Axis 状态更新
```

------

------

# 🥉 第三层：状态投影（UI关心）

------

```cpp
TEST(AxisViewModelCoreTest, StateShouldReflectAxisState)
```

------

```text
Given:
    Axis.state = Moving
Then:
    vm.state() == "Moving"
```

------

------

```cpp
TEST(AxisViewModelCoreTest, PositionShouldReflectAxisFeedback)
```

------

------

# 🏅 第四层：约束验证（防止写错）

------

```cpp
TEST(AxisViewModelCoreTest, ShouldNotModifyAxisDirectly)
```

------

👉 方式：

```text
检查：
vm 不调用 axis.xxx（除 applyFeedback / takeIntent）
```

------

------

```cpp
TEST(AxisViewModelCoreTest, ShouldUseOrchestratorForJog)
```

------

👉 验证：

```text
vm.jogPositivePressed()

→ JogOrchestrator 被调用
而不是 axis.jog()
```

------

------

# 🧪 五、Mock / Fake 设计（关键）

------

你需要：

------

## 1️⃣ FakeDriver（已有）

------

## 2️⃣ FakeOrchestrator（建议新增）

------

```cpp
class FakeJogOrchestrator {
public:
    bool startCalled = false;
    bool stopCalled = false;

    void start(Direction d) {
        startCalled = true;
    }

    void stop(Direction d) {
        stopCalled = true;
    }
};
```

------

------

## 3️⃣ Axis（真实即可）

👉 因为你 Domain 已经很稳定

------

------

# 🧠 六、测试结构建议

------

```text
tests/
  viewmodel/
    test_axis_viewmodel_core.cpp
```

------

------

# 📊 七、红灯覆盖范围（你要达到的）

------

```text
✔ Jog 行为（start/stop）
✔ Move 行为
✔ Stop 行为
✔ tick 驱动
✔ intent → driver
✔ feedback → axis
✔ 状态映射
```

------

👉 这 7 类覆盖完：

```text
ViewModel 就是“安全的”
```

------

------

# 🚨 八、一个你必须避免的坑

------

## ❌ 不要测试：

```text
✔ signal 是否发出
✔ Q_PROPERTY 是否更新
✔ QML行为
```

------

👉 这些属于：

```text
集成/UI测试，不是VM核心测试
```

------

------

# 🎯 九、你现在的正确开发顺序

------

```text
Step 1：写 AxisViewModelCore（无Qt）
Step 2：写 gtest（红灯）
Step 3：实现逻辑（绿灯）
Step 4：再包一层 QtAxisViewModel
```

------

------

# ✅ 十、一句话总结

------

```text
AxisViewModel 的 TDD 核心是：

👉 测“系统控制行为”，而不是“Qt表现形式”
```

------

------

# 🔥 如果你想更快推进

我可以帮你下一步：

👉 **把完整 test_axis_viewmodel_core.cpp（红灯版）直接写出来**

你可以直接复制 → 开发 → 绿灯。