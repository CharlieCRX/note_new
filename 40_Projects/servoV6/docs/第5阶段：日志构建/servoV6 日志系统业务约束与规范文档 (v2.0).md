------

# 📘 servoV6 日志系统业务约束与规范文档 (v2.0)

------

# 一、系统定位（重新定义）

```text
日志系统 = servoV6 的“可观测性基础设施（Observability Core）”
```

------

## 🎯 v2.0 核心目标

```text
✔ 可追踪（Traceable）——能还原完整操作链路
✔ 可理解（Readable）——像“业务故事”一样清晰
✔ 可定位（Diagnosable）——快速定位层级 + 问题点
✔ 可控制（Controllable）——不会产生性能灾难
✔ 可扩展（Extensible）——支持多轴、多组、多设备
```

------

# 二、核心模型（v2.0 新增🔥）

------

# 🟢 1️⃣ LogContext（日志上下文，必须引入）

------

```cpp
struct LogContext {
    GroupId group;      // 哪个系统
    AxisId axis;        // 哪个轴
    std::string traceId;// 本次操作链路ID
};
```

------

## 🎯 作用

```text
解决：

👉 多轴日志混乱
👉 多group日志混乱
👉 无法追踪一次操作
```

------

## 🔥 强制规则

```text
✔ 所有 Application / Infra 日志必须带 LogContext
✔ traceId 必须贯穿一次完整操作生命周期
```

------

------

# 🟡 2️⃣ 生命周期模型（v2.0新增🔥）

------

## 🎯 每一个“操作”必须具备生命周期

```text
START → RUNNING → DONE / ABORTED / ERROR
```

------

## 示例

```text
[INFO][APP][AbsOrch][G1][Y][T123] START MoveAbsolute target=100
[DEBUG][APP][AbsOrch][G1][Y][T123] Step: Ensuring → Move
[ERROR][APP][AbsOrch][G1][Y][T123] ERROR LimitTriggered
[SUMMARY][APP][AbsOrch][G1][Y][T123] FAILED at pos=80
```

------

## ❗强制约束

```text
✔ 每个 Orchestrator 必须打印 START
✔ 必须打印 END（Done / Error / Aborted）
✔ 不允许“无结尾操作”
```

------

------

# 🟣 3️⃣ Summary层（v2.0新增🔥）

------

## 🎯 定义

```text
Summary = 一次操作的“业务结论”
```

------

## 示例

```text
[SUMMARY][G1][Y][T123]
MoveAbsolute(100) → FAILED (LimitTriggered at 80)
```

------

## 使用场景

```text
✔ 现场调试
✔ 日志回放
✔ 统计分析
✔ 报表输出
```

------

------

# 三、分层日志语义（升级版）

------

## 🟣 1️⃣ Domain 层

------

### 🎯 基调

```text
Silent + Hook（不主动打印，但允许状态回调）
```

------

### 允许行为

```text
✔ 状态变化回调（onStateChanged）
✔ 严重逻辑错误（ERROR）
```

------

### 禁止行为

```text
❌ 不允许 INFO / DEBUG
❌ 不记录业务流程
```

------

------

## 🔵 2️⃣ Application 层（核心🔥）

------

### 🎯 基调

```text
系统“故事讲述者”
```

------

### 必须记录

------

#### 1️⃣ 生命周期

```text
START / DONE / ERROR / ABORTED
```

------

#### 2️⃣ Orchestrator Step（DEBUG）

```text
Step: Ensuring → Issuing → Waiting
```

------

#### 3️⃣ UseCase拒绝（WARN）

```text
Move rejected: AxisBusy
```

------

------

## 🟠 3️⃣ Infrastructure 层

------

### 🎯 基调

```text
物理世界观测层
```

------

### 必须记录

------

#### Driver

```text
TRACE → send command
ERROR → 通讯失败
```

------

#### PLC

```text
TRACE → tick（可采样）
WARN  → 限位触发
ERROR → 硬件异常
```

------

------

## 🟡 4️⃣ Presentation 层

------

### 🎯 基调

```text
用户行为记录
```

------

### 必须记录

```text
✔ 用户输入（INFO）
✔ ViewModel异常（ERROR）
```

------

------

# 四、日志结构规范（升级版）

------

## 🎯 标准格式

```text
[时间][级别][层][模块][Group][Axis][TraceId] 内容
```

------

## 示例

```text
[18:30:00.161][INFO][APP][AbsOrch][G1][Y][T123] START MoveAbsolute target=100
```

------

## 层级标识

```text
UI   → Presentation
APP  → Application
DOM  → Domain
HAL  → Infrastructure
```

------

------

# 五、日志级别定义（保持但强化）

------

| 级别            | 含义          | 场景                  |
| --------------- | ------------- | --------------------- |
| ERROR           | 系统/物理失败 | 硬件报警、状态异常    |
| WARN            | 业务拒绝      | Domain拒绝、预检失败  |
| INFO            | 关键事件      | START / DONE / UI操作 |
| DEBUG           | 状态变化      | Step迁移              |
| TRACE           | 高频数据      | PLC tick / pos变化    |
| SUMMARY（新增） | 操作总结      | 一次操作最终结果      |

------

------

# 六、性能控制机制（v2.0新增🔥）

------

## ❗必须实现

------

### 1️⃣ TRACE采样

```cpp
LOG_TRACE_EVERY_N(10)
```

------

### 2️⃣ 动态开关

```cpp
LOG_TRACE_IF(enabled)
```

------

### 3️⃣ 限速（Rate Limit）

```cpp
LOG_WARN_RATE_LIMIT(1s)
```

------

------

## 🎯 原因

```text
防止：

👉 PLC tick → 日志风暴
👉 IO阻塞 → 系统卡死
```

------

------

# 七、TraceId 生命周期（关键）

------

## 🎯 生成

```text
由 ViewModel 在用户触发操作时生成
```

------

## 🎯 传递路径

```text
UI → ViewModel → Orchestrator → UseCase → Driver
```

------

## ❗约束

```text
✔ 同一操作必须共享同一 traceId
✔ 不允许中途丢失
```

------

------

# 八、日志输出层级控制

------

## 🎯 推荐默认配置

```text
Production：
INFO + WARN + ERROR + SUMMARY

Debug：
DEBUG + INFO + WARN + ERROR

Deep Debug：
TRACE + 全量
```

------

------

# 九、示例（完整v2.0日志）

------

```text
[18:30:00.010][INFO][UI][AxisVM][G1][Y][T123] User MoveAbsolute 100

[18:30:00.010][INFO][APP][AbsOrch][G1][Y][T123] START MoveAbsolute target=100

[18:30:00.011][DEBUG][APP][AbsOrch][G1][Y][T123] Step: Initial → EnsuringEnabled

[18:30:00.012][TRACE][HAL][Driver][G1][Y][T123] Send EnableCommand

[18:30:00.170][TRACE][HAL][PLC][G1][Y][T123] Tick pos=0.5

[18:30:01.760][ERROR][HAL][PLC][G1][Y][T123] LIMIT TRIGGERED at 80

[18:30:01.762][ERROR][APP][AbsOrch][G1][Y][T123] Motion aborted due to Error

[18:30:01.763][SUMMARY][APP][AbsOrch][G1][Y][T123]
MoveAbsolute(100) → FAILED (LimitTriggered at 80)
```

------

# 十、设计原则总结（写在文档最后）

------

```text
1. 日志是系统的“时间线”
2. TraceId 是“因果链”
3. Application 是“故事讲述者”
4. Infrastructure 是“物理记录仪”
5. Summary 是“结论”
```

------

# 🎯 最终一句话总结

------

```text
v1.0 的日志系统：

👉 能讲清楚“发生了什么”

v2.0 的日志系统：

👉 能还原“为什么发生 + 属于谁 + 如何结束”
```

------

# 🚀 如果你下一步继续推进（建议）

我可以帮你直接做：

👉 **这套日志系统的完整 C++ 实现（含宏 + RAII TraceScope + 自动TraceId传递）**

而且会完全贴合你现在的：

- Orchestrator
- UseCase
- FakePLC

------

你只需要说：

```text
👉 帮我把 v2.0 日志系统落成代码
```

我会直接带你落地（不是讲概念，是写你能用的代码）。