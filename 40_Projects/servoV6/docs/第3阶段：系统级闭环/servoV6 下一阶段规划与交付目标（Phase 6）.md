# 📘 servoV6 下一阶段规划与交付目标（Phase 6）

------

# 一、当前阶段判断（你现在的位置）

基于你代码 + 测试 +提交记录：

👉

------

## 🎯 结论

```text
你已经完成：

✔ Phase 1：Domain建模
✔ Phase 2：UseCase分层
✔ Phase 3：Driver/HAL抽象
✔ Phase 4：Feedback闭环
✔ Phase 5：SystemIntegration（端到端验证）
```

------

## 🔥 当前系统能力

```text
✔ 单轴完整控制系统（工业语义）
✔ Intent + Feedback 闭环
✔ Orchestrator（含可观测性）
✔ FakePLC（物理模拟）
✔ 149+ 自动化测试
✔ 安全/异常/恢复路径已覆盖
```

------

## ⚠️ 当前短板（非常关键）

你现在缺的不是“功能”，而是：

```text
❌ 没有“用户入口”（UI / API）
❌ 没有“系统外部可观测性”
❌ 没有“交互语义封装”
❌ 系统还不能被“人用”
```

------

# 🎯 二、下一阶段目标（Phase 6）

------

# 🧭 阶段名称

```text
Presentation & Interaction Layer（表现与交互层）
```

------

# 🎯 核心目标（一句话）

```text
把“可验证的控制系统”

→

变成“可操作的控制产品”
```

------

# 三、阶段核心交付物（必须完成）

------

# 🥇 交付物1：AxisViewModel（核心）

------

## 🎯 定义

```text
AxisViewModel = Domain + Application 的“对外接口”
```

------

## 必须提供能力：

------

### 1️⃣ 状态输出（给UI）

```cpp
AxisState state;
double absPos;
double relPos;
double jogVelocity;
double moveVelocity;
bool isEnabled;
bool hasError;
QString errorMessage;
```

------

------

### 2️⃣ 控制输入（来自UI）

```cpp
void jogPositivePressed();
void jogPositiveReleased();

void jogNegativePressed();
void jogNegativeReleased();

void moveAbsolute(double pos);
void moveRelative(double delta);

void stop();
void enable();
void disable();
```

------

👉 本质：

```text
UI 不直接调用 UseCase
→ 通过 ViewModel
```

------

------

### 3️⃣ 状态同步接口

```cpp
void tick();
```

内部执行：

```text
Orchestrator → Driver → FakePLC → Feedback → Axis
```

------

------

# 🥈 交付物2：QtQuick 控制面板（UI）

------

## 🎯 最小可交付界面（MVP）

------

### 必须包含：

```text
✔ 当前位置显示（abs / rel）
✔ 当前状态（Idle / Moving / Jogging / Error）
✔ Enable 状态灯
✔ 正/反点动按钮（按下/松开）
✔ 定位输入框（abs / rel）
✔ Stop按钮
✔ 报警显示
```

------

------

### UI交互要求

```text
✔ 按下 → jog start
✔ 松开 → jog stop
✔ 按钮互斥（防误操作）
✔ 输入合法性校验
```

------

------

# 🥉 交付物3：系统运行Demo（关键）

------

## 🎯 你必须能展示：

------

### 场景1：点动

```text
按住 → 连续运动
松开 → 停止
```

------

### 场景2：定位

```text
输入位置 → 自动执行
```

------

### 场景3：异常

```text
触发限位 → 自动停止 + 报警
```

------

👉 这就是：

```text
“可演示系统”
```

------

# 🏅 四、验证方案（必须可验收）

------

# 🧪 验证1：ViewModel 单元测试

------

## 示例

```cpp
TEST(AxisViewModelTest, ShouldStartJogWhenButtonPressed)
```

------

验证：

```text
✔ 调用了 JogOrchestrator
✔ Driver发送正确命令
```

------

------

# 🧪 验证2：UI行为测试（逻辑层）

------

```text
✔ 按下 → jog start
✔ 松开 → jog stop
✔ 输入非法值 → 拒绝
```

------

------

# 🧪 验证3：系统运行一致性

------

## 场景：

```text
UI → ViewModel → 系统 → FakePLC → UI刷新
```

------

验证：

```text
✔ UI显示与Axis状态一致
✔ 无“假完成”
✔ 无状态跳变错误
```

------

------

# 🧪 验证4：回归测试（必须）

------

```text
✔ 所有现有 149 tests 必须通过
```

------

👉 这保证：

```text
UI层不会破坏系统核心
```

------

# 五、阶段执行计划（建议）

------

## Day 1~2

```text
✔ 设计 AxisViewModel
✔ 写 ViewModel 单元测试
```

------

## Day 3~4

```text
✔ 实现 QtQuick UI
✔ 绑定 ViewModel
```

------

## Day 5

```text
✔ 跑通完整 Demo
✔ 修复交互问题
```

------

------

# 六、阶段完成标准（必须满足）

------

## ✅ 技术标准

```text
✔ UI 可操作完整流程
✔ 所有命令通过 ViewModel
✔ 无直接访问 Domain
```

------

## ✅ 业务标准

```text
✔ 点动可控
✔ 定位可靠
✔ 异常可见
```

------

## ✅ 工程标准

```text
✔ 所有测试通过
✔ 无崩溃
✔ 行为一致
```

------

------

# 七、下一阶段之后（提前给你方向）

------

## Phase 7（你很快会进入）

```text
Multi-Axis（多轴系统）
```

------

包括：

```text
✔ X1X2联动
✔ 同步控制
✔ 超差检测
✔ 轨迹规划
```

------

👉 但前提是：

```text
单轴 UI 已稳定
```

------

# 🧠 八、你现在最重要的转变

------

你已经完成：

```text
写“工业控制系统”
```

------

你下一步必须完成：

```text
做“可交互的产品”
```

------

👉 这是两个完全不同的能力层级。

------

# 🔥 九、我给你的最终建议（非常直接）

------

## ❗只做这一件事：

```text
先做 AxisViewModel
```

------

因为：

```text
它是：

Domain → UI 的唯一桥梁
```

------

# ✅ 十、一句话总结

------

```text
你现在不缺系统，

你缺的是：

👉 一个让人能“用起来”的入口
```

------

------

如果你愿意，我下一步可以帮你：

👉 **把 AxisViewModel 直接设计出来（包含接口 + 状态流 + Qt绑定）**

这一步会让你 UI 开发直接起飞。