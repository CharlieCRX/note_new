# 🚀 servoV6 阶段总结（Phase 1：Axis Domain Core）

## 一、阶段目标

本阶段目标：

```text
构建一个“可测试、可扩展、与硬件解耦”的轴控制核心模型（Domain Core）
```

核心要求：

- 不依赖 PLC / 硬件
- 支持完整控制语义（Jog / Move / Stop / Zero）
- 具备工业级状态机约束
- 可通过 TDD 完整验证

------

## 二、核心成果（最重要）

### ✅ 1. 构建 Axis 领域模型（Domain Entity）

完成：

- Axis 实体建模
- AxisState 状态机
- AxisFeedback（现实输入）
- AxisCommand（意图输出）

👉 本质：

```text
Axis = 状态 + 行为 + 规则
```

------

### ✅ 2. 建立 Intent / Reality 双通道模型

实现：

- 用户操作 → 生成 Command（Intent）
- PLC反馈 → applyFeedback（Reality）

👉 核心思想：

```text
控制 ≠ 状态
Intent ≠ Reality
```

------

### ✅ 3. 构建完整状态机（工业级）

状态定义：

```text
Unknown / Disabled / Idle / Jogging / MovingAbsolute / MovingRelative / Error
```

实现约束：

- 非 Idle 状态禁止新命令
- Error 状态锁死
- 执行中屏蔽输入（Shielding）
- Stop 具有最高优先级（可穿透）

------

### ✅ 4. 统一命令系统（variant + 单一意图槽）

实现：

```cpp
std::variant<...> AxisCommand
```

能力：

- 单一命令槽（避免冲突）
- 自动覆盖（variant）
- 生命周期管理（创建 → 执行 → 消失）

------

### ✅ 5. 命令生命周期闭环（核心突破）

实现完整流程：

```text
创建 → 执行中 → 完成 → 清除
```

闭环条件：

- 状态达标（Idle）
- 数值达标（位置进入容差）

------

### ✅ 6. 数值收敛系统（工业控制核心）

实现：

- 绝对定位闭环（abs）
- 相对定位闭环（rel）
- 容差判断（epsilon）
- 相对定位起点捕获（startAbs）

👉 本质：

```text
控制成功 = 状态 + 数值 双重达标
```

------

### ✅ 7. 坐标体系建模（关键能力）

实现：

- 绝对坐标（absPos）
- 相对坐标（relPos）
- 相对零点基准（relZeroAbsPos）

关键约束：

```text
所有闭环基于绝对坐标判断
```

------

### ✅ 8. 安全控制系统（工业级）

#### 软限位：

- 数值限位（posLimitValue / negLimitValue）
- 状态限位（posLimit / negLimit）

#### 行为约束：

- 越界拒绝 Move
- 限位状态只允许反向 Jog（逃逸机制）
- 运行中触发限位 → 强制取消任务

------

### ✅ 9. Stop 控制机制（抢占式设计）

实现：

- Stop 可穿透所有状态
- 覆盖当前命令
- 独立生命周期闭环

------

### ✅ 10. 坐标操作系统

实现：

- 绝对位置清零（ZeroAbsolute）
- 设置相对零点
- 清除相对零点

关键设计：

```text
双条件闭环（位置 + 基准）
```

避免“半完成状态”

------

## 三、测试体系（TDD 成果）

### ✅ 覆盖范围：

- 状态机约束
- 命令生成与拒绝
- 生命周期闭环
- 数值收敛逻辑
- 坐标系统
- 限位与安全逻辑

### ✅ 特点：

```text
✔ 所有行为由测试定义（Specification）
✔ 无需 PLC 即可验证
✔ Domain 层接近 100% 覆盖
```

------

## 四、架构达成情况

当前已实现：

```text
[Domain] ✅ 完成
Axis（核心实体）
Command系统
状态机
规则引擎
```

尚未实现：

```text
[Application] ❌（UseCase）
[Infrastructure] ❌（Driver / PLC）
[Presentation] ❌（ViewModel）
```

------

## 五、关键设计原则（本阶段沉淀）

### 1️⃣ Reality First

```text
状态必须来自 PLC（applyFeedback）
```

------

### 2️⃣ 状态驱动系统

```text
行为由状态决定，而不是命令决定
```

------

### 3️⃣ 命令不可变

```text
执行中参数冻结
```

------

### 4️⃣ Intent / Reality 解耦

```text
Domain 只产生意图，不负责执行
```

------

### 5️⃣ 单一职责分离

```text
Axis：做决策（是否允许）
Driver：做执行（如何实现）
UseCase：做流程（如何编排）
```

------

### 6️⃣ 强一致性闭环

```text
完成 = 状态达标 + 数值达标
```

------

## 六、当前系统本质（核心认知）

```text
这不是一个“控制类”

而是一个：

👉 基于状态机 + 数值闭环 + 意图驱动的控制核心引擎
```

------

## 七、阶段定位

当前进度：

```text
Phase 1：Domain Core（完成 ✅）
Phase 2：UseCase（下一步）
Phase 3：Driver / PLC
Phase 4：UI 联通
Phase 5：多轴扩展
```

------

## 八、下一阶段目标（明确方向）

下一步重点：

```text
1. 构建 Application（UseCase）
2. 引入 Driver（Fake → PLC）
3. 打通 Intent → 执行链路
4. 构建反馈同步链路（Gateway + Sync）
```

------

## 九、能力提升总结（非常关键）

本阶段提升：

```text
✔ 状态机建模能力
✔ 领域建模能力（Domain）
✔ TDD 实践能力
✔ 工业控制抽象能力
✔ 系统分层设计能力
```

------

## 十、一句话总结

```text
本阶段完成了 servoV6 的“控制核心引擎”，

实现了从“控制逻辑”到“控制系统”的跃迁。
```

------