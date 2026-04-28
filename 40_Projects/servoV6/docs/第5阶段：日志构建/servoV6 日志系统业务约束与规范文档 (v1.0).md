这是一个非常成熟且克制的决定。在写代码之前先定义**“日志的业务语义与约束”**，是高级架构师的标配做法。

日志不仅是给机器看的，更是给未来的你和团队看的“系统运行故事”。根据你当前 `servoV6` 严密的 Clean Architecture 架构，我为你整理了这份**《servoV6 日志系统业务约束与规范文档 (v1.0)》**，并在文末给出了最终的模拟展示效果。

------

# 📘 servoV6 日志系统业务约束与规范文档 (v1.0)

## 一、 日志系统核心定位

Plaintext

```
日志不是 `printf`，它是系统的“飞行黑匣子”。
```

在 servoV6 中，日志系统必须严格遵守**整洁架构的依赖倒置原则**：

- **只准外层依赖日志接口，不准核心层依赖具体实现。**
- **日志必须呈现出与代码结构一致的“层次感”。**

------

## 二、 分层日志语义约束（The Layered Semantics）

每一层能且只能打印属于自己职责范围内的日志。绝对禁止跨层记录（例如 UI 层禁止打印物理坐标变化）。

### 🟣 1. Domain 层 (实体与业务规则)

- **基调**：**极度静默 (Silent)**。
- **约束**：
  - Domain 层是纯粹的逻辑推演，**不允许**包含任何常规流程日志（INFO/DEBUG）。
  - **唯一允许**：当发生严重违背物理定律或状态机内部错乱时（例如 `applyFeedback` 收到了不可能的状态组合），记录 `FATAL/ERROR`。
  - 正常的拦截（如 `jog` 撞限位）**不准打印日志**，只需返回 `RejectionReason`，由 Application 层记录。

### 🔵 2. Application 层 (用例与编排器)

- **基调**：**系统的“故事讲述者” (Storyteller)**。
- **约束**：
  - **Orchestrator (编排器)**：必须记录状态机（Step）的每一次跃迁（`DEBUG` 级别）。这是系统调试的核心。
  - **UseCase (执行器)**：当意图被 Domain 拒绝（返回 `RejectionReason != None`）时，必须记录 `WARN`。
  - 核心业务里程碑（如：开始定位、到达目标、完成动作）记录为 `INFO`。

### 🟠 3. Infrastructure 层 (驱动与防腐层)

- **基调**：**数据的“轰炸区” (Engine Room)**。
- **约束**：
  - **FakePLC / Driver**：负责记录物理世界的每一滴变化。Tick 循环内的坐标、速度推演必须用 `TRACE` 级别包裹（方便在生产环境一键关闭，防止 IO 爆炸）。
  - 模拟硬件异常（如触发限位、收到未知指令）必须记录 `WARN` 或 `ERROR`。

### 🟡 4. Presentation 层 (ViewModel)

- **基调**：**用户的“交互侧写” (Observer)**。
- **约束**：
  - 记录 UI 下发的所有指令意图（如 `jogPositivePressed`），级别为 `INFO`。
  - 记录状态映射层面的严重异常（如从 C++ 抛到 QML 时的类型不匹配）。

------

## 三、 日志级别与熔断约束 (Log Levels)

| **级别**    | **语义定义**         | **servoV6 具体场景映射**                                     |
| ----------- | -------------------- | ------------------------------------------------------------ |
| 🔴 **ERROR** | **系统/物理级阻断**  | 硬件报警 (`AxisState::Error`)、Orchestrator 兜底失败、系统死锁。 |
| 🟠 **WARN**  | **业务级拒绝与拦截** | 指令被 Domain 拒绝（如运动中断电、超限位预检失败）。         |
| 🟢 **INFO**  | **粗粒度业务里程碑** | VM收到用户指令、Orchestrator启动任务、Orchestrator任务 `Done`。 |
| 🔵 **DEBUG** | **细粒度状态机跃迁** | `EnsuringEnabled` -> `IssuingMove` -> `WaitingMotionStart`。 |
| ⚪ **TRACE** | **高频底层物理数据** | FakePLC 的每 10ms Tick：当前绝对坐标、当前相对坐标、当前速度。 |

------

## 四、 结构化 Tag 命名规范 (Formatting)

为了让日志具备极强的可检索性（使用 `grep` 或日志收集工具时一目了然），日志必须遵循以下前缀结构：

Plaintext

```
[时间戳] [级别] [层级标识:模块名] 具体信息
```

**层级标识强制规范：**

- `[UI:xxx]` -> Presentation 层 (如 `[UI:AxisVM]`)
- `[APP:xxx]` -> Application 层 (如 `[APP:AbsOrch]`, `[APP:JogUc]`)
- `[DOM:xxx]` -> Domain 层 (如 `[DOM:Axis]`)
- `[HAL:xxx]` -> Infrastructure/Hardware Abstraction (如 `[HAL:FakePLC]`, `[HAL:Driver]`)

------

## 五、 最终模拟展示效果 (Simulated Output)

假设此时系统运行，用户在界面上**输入了绝对定位 `100.0`，但是在运动到 `80.0` 时触发了 FakePLC 模拟的硬件限位报警**。

以下是开启了全级别日志（包含 TRACE）的终端输出故事：

Plaintext

```
# --- 用户在 UI 触发绝对定位指令 ---
[2026-04-24 18:30:00.010] [INFO ] [UI:AxisVM] User requested MoveAbsolute, target: 100.0 mm
[2026-04-24 18:30:00.010] [INFO ] [APP:AbsOrch] MoveAbsoluteOrchestrator started, target = 100.0
[2026-04-24 18:30:00.010] [DEBUG] [APP:AbsOrch] Step changed: Initial -> EnsuringEnabled

# --- Orchestrator 发现未上电，主动发起上电 ---
[2026-04-24 18:30:00.011] [INFO ] [APP:EnableUC] Executing Enable(true)
[2026-04-24 18:30:00.011] [TRACE] [HAL:Driver] Sending EnableCommand(active=true)
[2026-04-24 18:30:00.011] [TRACE] [HAL:FakePLC] Received EnableCommand, simulating hardware delay...

# --- FakePLC 的 Tick 循环 (高频过滤展示) ---
[2026-04-24 18:30:00.020] [TRACE] [HAL:FakePLC] Tick 10ms | State: Unknown | Pos: 0.000
[2026-04-24 18:30:00.160] [TRACE] [HAL:FakePLC] Hardware delay finished. State -> Idle.

# --- 状态流转到发指令 ---
[2026-04-24 18:30:00.161] [DEBUG] [APP:AbsOrch] Axis is Idle. Step changed: EnsuringEnabled -> IssuingMove
[2026-04-24 18:30:00.161] [INFO ] [APP:MoveAbsUC] Executing move to target 100.0
[2026-04-24 18:30:00.162] [TRACE] [HAL:Driver] Sending MoveCommand(Absolute, target=100.0)
[2026-04-24 18:30:00.162] [DEBUG] [APP:AbsOrch] Move issued. Step changed: IssuingMove -> WaitingMotionStart

# --- 物理运动开始 ---
[2026-04-24 18:30:00.170] [TRACE] [HAL:FakePLC] Tick 10ms | State: MovingAbs | Pos: 0.500
[2026-04-24 18:30:00.171] [DEBUG] [APP:AbsOrch] Motion observed. Step changed: WaitingMotionStart -> WaitingMotionFinish
[2026-04-24 18:30:00.670] [TRACE] [HAL:FakePLC] Tick 10ms | State: MovingAbs | Pos: 25.500
[2026-04-24 18:30:01.170] [TRACE] [HAL:FakePLC] Tick 10ms | State: MovingAbs | Pos: 50.500

# --- 🚨 突发状况：FakePLC 撞击限位 ---
[2026-04-24 18:30:01.760] [TRACE] [HAL:FakePLC] Tick 10ms | State: MovingAbs | Pos: 80.000
[2026-04-24 18:30:01.761] [ERROR] [HAL:FakePLC] 💥 HARDWARE LIMIT TRIGGERED at Pos: 80.0! Forcing axis to Error state!

# --- 异常被反馈同步，Orchestrator 捕获并熔断 ---
[2026-04-24 18:30:01.762] [ERROR] [APP:AbsOrch] Axis dropped into Error state! Aborting motion!
[2026-04-24 18:30:01.762] [DEBUG] [APP:AbsOrch] Step changed: WaitingMotionFinish -> Error
[2026-04-24 18:30:01.763] [WARN ] [UI:AxisVM] Axis reported Error. Motion orchestrator aborted.
```

------

### 💡 架构师点评

你看上面这段模拟输出，它已经**不再是干巴巴的代码堆砌，而是一部完美的“业务电影”**。

- 如果某天系统卡死了，你只需要看 `[DEBUG] [APP:AbsOrch]` 停在了哪一步（比如一直停在 `WaitingMotionFinish`）。
- 如果界面点了没反应，你只需要看 `[APP:MoveAbsUC]` 是不是打出了一条 `[WARN]` 说被 `InvalidState` 拒绝了。

有了这份约束，我们下一步实现时，无论是手写轻量级 Facade 还是引入 `spdlog`，代码都会非常清晰且极具工业水准。你认可这份语义规范吗？