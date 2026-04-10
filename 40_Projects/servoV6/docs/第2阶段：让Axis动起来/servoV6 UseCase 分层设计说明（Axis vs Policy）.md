# servoV6 UseCase 分层设计说明（Axis vs Policy）

------

# 🎯 一、设计结论（先给答案）

```text
UseCase 必须拆为两层：

1️⃣ axis/   —— 纯动作（Stateless）
2️⃣ policy/ —— 行为策略（Stateful / Orchestration）
```

最终结构：

```text
application/

├── axis/
│   ├── JogAxisUseCase        ✅ 纯净
│   ├── MoveAbsoluteUseCase   ✅ 纯净
│   └── StopUseCase           ✅ 纯净

├── policy/
│   ├── AutoEnablePolicy
│   ├── AutoDisablePolicy
│   └── MoveTaskOrchestrator
```

------

# 🧠 二、为什么必须这样拆？

------

## ❗问题来源（你已经踩到了）

当前 JogUseCase 存在如下逻辑：

```text
Jog失败 → 自动 Enable → 返回成功
```

这导致：

```text
1. UseCase 语义被污染
2. 行为不可预测
3. 测试不再可靠
4. UI认知错误（以为Jog成功）
```

👉 本质问题：

```text
“动作”和“策略”混在一起了
```

------

# 🧱 三、核心设计原则（必须遵守）

------

## 🎯 原则1：单一语义原则（Single Responsibility）

```text
一个 UseCase 只能表达一个语义
```

------

### ✅ 正确：

```text
JogUseCase = “尝试点动”
```

------

### ❌ 错误：

```text
JogUseCase = “点动 or 上电 or 修复状态”
```

------

## 🎯 原则2：行为一致性（Determinism）

```text
同一个输入 → 必须产生同一个类型的输出
```

------

### ❌ 当前问题：

```text
execute(Jog):

Idle      → JogCommand
Disabled  → EnableCommand   ❌
Error     → 无指令
```

👉 这是工业系统大忌

------

## 🎯 原则3：策略不可内嵌（Policy Separation）

```text
“是否 Enable” ≠ “能不能 Jog”
```

------

👉 这两件事：

```text
Domain 决定：能不能动
Policy 决定：要不要上电
```

------

# 🧠 四、两层模型的本质区别

------

# 🟢 第一层：axis/（纯动作层）

------

## 🎯 定义

```text
“设备能做什么”
```

------

## 🧾 特征

```text
✔ 无状态（不持久化流程）
✔ 不跨时间
✔ 不做等待
✔ 不做自动补偿
✔ 不组合多个行为
```

------

## 🧠 示例

```text
JogAxisUseCase：
    axis.jog() → send

MoveAbsoluteUseCase：
    axis.move() → send
```

------

## ❗语义保证

```text
✔ 成功 = 已发送对应命令
✔ 失败 = Domain拒绝
✔ 不会“偷偷做别的事”
```

------

------

# 🔵 第二层：policy/（策略层）

------

## 🎯 定义

```text
“什么时候做 + 做多久 + 做完怎么办”
```

------

## 🧾 特征

```text
✔ 有状态（Stateful）
✔ 跨时间（依赖反馈）
✔ 负责流程编排
✔ 可以组合多个 UseCase
✔ 可以引入策略（enable/disable）
```

------

## 🧠 示例

------

### 1️⃣ AutoEnablePolicy

```text
当执行动作前：

if Disabled → Enable
```

------

### 2️⃣ AutoDisablePolicy

```text
当满足：

✔ Idle
✔ velocity == 0
✔ 无 pending command

→ Disable
```

------

### 3️⃣ MoveTaskOrchestrator

```text
完整流程：

Enable → Move → 等待完成 → Disable
```

------

------

# 🧠 五、为什么不能把策略写进 axis UseCase？

------

## ❌ 原因1：策略是“可变的”

不同场景需求不同：

```text
✔ 实验模式 → 每次 Move 后 Disable
✔ 连续扫描 → 保持 Enable
✔ 调试模式 → 永远 Enable
```

------

👉 如果写死：

```text
系统不可扩展
```

------

------

## ❌ 原因2：破坏测试隔离

------

### 当前错误测试：

```text
ShouldSendEnableWhenDisabled ❌
```

问题：

```text
测试的是“策略”，不是“动作”
```

------

### 正确测试应该是：

```text
JogUseCaseTest：

Disabled → 拒绝
Idle     → 发送 Jog
```

------

👉 策略应该单独测试：

```text
AutoEnablePolicyTest
```

------

------

## ❌ 原因3：破坏架构边界

根据架构设计：

```text
Application 层职责：

✔ 定义流程
❌ 不混入业务规则
❌ 不混入设备控制策略
```

------

👉 如果混入：

```text
UseCase = Domain + Policy + Workflow（灾难）
```

------

------

# 🧠 六、设计带来的核心收益

------

## ✅ 1. 行为可预测

```text
Jog 永远只做 Jog
Move 永远只做 Move
```

------

## ✅ 2. 测试清晰

```text
axis/ 测试 → 纯逻辑
policy/ 测试 → 流程行为
```

------

## ✅ 3. 易扩展

未来新增：

```text
KeepEnabledPolicy
MultiAxisSyncPolicy
EnergySavingPolicy
```

👉 不需要改已有代码

------

## ✅ 4. 符合工业控制本质

```text
设备能力（Capability） ≠ 控制策略（Policy）
```

------

------

# 🧠 七、你当前场景的正确落点

------

你的需求：

```text
Move 后必须 Disable（降低噪声）
```

------

## ❗正确归属

```text
👉 Policy 层
```

------

## ❌ 错误归属

```text
MoveUseCase 内部
```

------

------

# 🧠 八、最终抽象（最重要的一句话）

------

```text
Axis = “我能做什么”
UseCase(axis) = “现在做什么”
Policy = “在什么条件下做 + 做完怎么办”
```

------

------

# 🎯 九、总结

------

## ❗你这次设计调整，本质是在完成：

```text
从“功能实现”
→
“系统建模”
```

------

## 最终模型：

```text
[ Domain ]
    Axis（规则）

        ↓

[ Application - axis ]
    单一动作（Jog / Move）

        ↓

[ Application - policy ]
    行为编排（Enable / Disable / 生命周期）

        ↓

[ Driver ]
    执行
```

------

# 🔥 最后一刀（核心认知）

```text
不要让 UseCase 变聪明

要让系统“组合出智能”
```

------

这就是为什么我们必须采用：

```text
axis + policy 分层模型
```