# PLC 设计问题导致 servoV6 程序复杂化分析

> 文档版本：v1.0  
> 日期：2026-07-15  
> 目的：系统梳理当前 PLC 层设计对上层应用（servoV6）造成的工程复杂性问题，并给出每项问题的"预期接口"，作为后续 PLC 固件改进的参考依据。

---

## 一、电机状态碎片化问题

### 1.1 问题描述

当前 PLC 中，一个电机的状态信息被**分散到多个不同类型的寄存器**中（以 X 轴为例）：

| 寄存器 | 地址（示意）    | 类型  | 含义                                         |
| ------ | --------------- | ----- | -------------------------------------------- |
| D100   | 轴 X 状态显示   | Int16 | 0=未使能, 1=使能(静止), 2=使能(运动), 3=报警 |
| M110   | 轴 X 绝对定位中 | Coil  | 绝对定位运动标志                             |
| M111   | 轴 X 相对定位中 | Coil  | 相对定位运动标志                             |
| M112   | 轴 X 点动中     | Coil  | 点动运动标志                                 |
| D110   | 轴 X 报警代码   | Int16 | 报警码（0=正常）                             |

### 1.2 对 servoV6 造成的影响

#### 1.2.1 必须实现"多信号融合推导引擎"

servoV6 不得不在 infrastructure 层专门实现 `AxisStateDeriver::deriveAxisState()` 函数（位于 `infrastructure/plc/AxisStateDeriver.h`），将 **5 个离散寄存器**融合为一个 `AxisState` 枚举：

```cpp
// servoV6 中的状态推导优先级链：
AxisState deriveAxisState(int16_t d100State, int16_t alarmCode,
                          bool absMoving, bool relMoving, bool jogging) {
    if (d100State == 3 || alarmCode != 0) return AxisState::Error;
    if (d100State == 0)                   return AxisState::Disabled;
    if (absMoving)                        return AxisState::MovingAbsolute;
    if (relMoving)                        return AxisState::MovingRelative;
    if (jogging)                          return AxisState::Jogging;
    return AxisState::Idle;
}
```

这个函数是一个"补丁"——它的存在本身就是对 PLC 接口设计缺陷的补救。

#### 1.2.2 D100 在 1↔2 之间抖动

多圈编码器的微小位置噪声导致 D100 在 "1（使能静止）" 和 "2（使能运动）" 之间反复跳动。`AxisStateDeriver` 的注释明确指出：

> "多圈编码器抖跳导致的 D100=1↔2 变化被此 if-else 链消除"

这意味着：

- PLC 自己的状态判定逻辑不稳定，把编码器噪声当作"运动"上报
- servoV6 必须用独立 Coil（M110/M111/M112）来**覆盖** D100 的判断，而不是 D100 本身就可靠
- 如果没有 M110~M112 辅助，servoV6 无法区分"使能静止"和"正在点动"

#### 1.2.3 报警码需要二次解析

当 `alarmCode == 3`（软限位报警）时，PLC 不直接告知是正限位还是负限位。servoV6 需要额外读取软限位值（SOFT_LIMIT_POS / SOFT_LIMIT_NEG），再与当前位置做比对：

```cpp
// ModbusSystemDriver::pollFeedback() 中的限位二次解析逻辑（~30 行代码）
if (alarmCode == 3) {
    if (static_cast<double>(absPos) + 0.1 >= static_cast<double>(softLimitPos)) {
        posLimit = true;
    }
    if (static_cast<double>(absPos) - 0.1 <= static_cast<double>(softLimitNeg)) {
        negLimit = true;
    }
}
```

这段逻辑存在边界判断误差（+/- 0.1 的魔数），且本质上是把本该 PLC 完成的判断推到了上位机。

### 1.3 预期接口

PLC 应为每个轴提供一个**唯一、综合的状态寄存器**（例如 `AXIS_STATE_COMBINED`），其取值语义清晰：

```
0 → 未使能 (Disabled)
1 → 使能中（空闲）(Idle)
2 → 点动中 (Jogging)
3 → 相对定位移动中 (MovingRelative)
4 → 绝对定位移动中 (MovingAbsolute)
5 → 报警 (Error)
```

当状态为报警（5）时，同时提供 `AXIS_ALARM_CODE` 寄存器，通过编码区分具体报警类型（如 1=过流, 2=过载, 3=正限位, 4=负限位, ...）。

**核心收益**：servoV6 的 `AxisStateDeriver` 可以删除，`pollFeedback()` 中约 50 行的融合和限位二次解析代码可以简化为一次寄存器读取。

---

## 二、使能的前置依赖（Enable Pre-requisite）

### 2.1 问题描述

当前 PLC 要求所有运动操作（点动、绝对定位、相对定位）之前，必须**先单独发送使能指令**，运动结束后还需**再发送掉电指令**。PLC 不会自动管理使能状态。

### 2.2 对 servoV6 造成的影响

#### 2.2.1 编排器状态机膨胀

以 `JogOrchestrator` 为例，一个简单的"摇杆推动→电机转动→摇杆松开→电机停止"操作，其状态机需要包含 **10 个步骤**：

```
Idle → EnsuringEnabled → PostEnableDelay(400ms) → IssuingJog → Jogging
     → IssuingStop → WaitingForIdle → PostStopDelay(500ms) → EnsuringDisabled → Done
```

其中 `EnsuringEnabled`、`PostEnableDelay`、`EnsuringDisabled`、`PostStopDelay` 四个步骤完全是由"PLC 不自动管理使能"导致的额外复杂度。

同样的模式在 `AutoAbsMoveOrchestrator` 和 `AutoRelMoveOrchestrator` 中重复出现。

#### 2.2.2 硬编码的时间延迟

servoV6 不得不在各编排器中硬编码 magic delay：

| 延迟               | 值     | 位置                                        | 原因                           |
| ------------------ | ------ | ------------------------------------------- | ------------------------------ |
| 使能后硬件稳定延迟 | 400ms  | `JogOrchestrator::m_postEnableDelaySeconds` | 等待抱闸释放、磁场建立         |
| 停止后完全静止延迟 | 500ms  | `JogOrchestrator::m_postStopDelaySeconds`   | 等待转速降到合理范围           |
| 使能超时           | 3000ms | `JogOrchestrator::m_enableTimeoutSeconds`   | 使能后未收到 Idle 反馈判定失败 |
| 边沿触发脉冲宽度   | 150ms  | `ModbusSystemDriver::EDGE_TRIGGER_PULSE_MS` | 写 ON→延迟→写 OFF              |

这些延迟值：

- 在不同 PLC 型号/固件版本下可能需要调整
- 与 PLC 的加减速时间设置耦合
- 过短会导致运动不完整，过长会降低响应速度

#### 2.2.3 异常路径的额外复杂度

当编排器运行中检测到 `AxisState::Error`，需要进入 `ErrorDisabling` 步骤——带重试的掉电流程：

```
ErrorDisabling → 发送 Disable → 等待 Disabled 反馈 → 超时(3s) → 重试(最多2次) → Error 终态
```

这个步骤包含 4 个成员变量（`m_errorDisableSent`、`m_errorDisableRetryCount`、`m_errorDisableSentTime`、`kMaxErrorDisableRetries`）来管理重试逻辑。如果 PLC 能在检测到报警时自动掉电，这整个步骤都可以删除。

#### 2.2.4 三个编排器中的重复代码

`JogOrchestrator`、`AutoAbsMoveOrchestrator`、`AutoRelMoveOrchestrator` 三者都包含几乎相同的 `EnsuringEnabled` 阶段（发送使能→等待 Idle），但它们是**独立实现**的，没有抽象出公共基类。这是因为使能流程与各自的运动语义耦合，难以干净地提取。

#### 2.2.5 X 轴的特殊分支

`JogOrchestrator::EnsuringDisabled` 中有一个 TODO 标记的特殊处理：

```cpp
// TODO 逻辑存在耦合地方，后期需要优化设计，抽象出一个独立的流程编排器来处理X轴的点动。
if (m_targetId == AxisId::X) {
    // X轴跳过关闭使能逻辑
    m_step = Step::Done;
    return;
}
```

这是因为龙门 X 轴由 X1/X2 物理轴联动，使能/掉电逻辑与单轴不同。这个 if 分支是对 PLC 接口不一致的妥协。

### 2.3 预期接口

PLC 应**透明屏蔽使能概念**，上层应用只需发送运动指令：

**修改后的点动流程**：

```
servoV6 写 JOG_FORWARD = ON   →  PLC 内部自动使能 → 开始点动
servoV6 写 JOG_FORWARD = OFF  →  PLC 内部自动停止 → 自动掉电
```

**修改后的绝对定位流程**：

```
servoV6 写 ABS_TARGET = 100.0  →  PLC 接收目标值
servoV6 写 ABS_TRIGGER = ON    →  PLC 自动使能 → 执行定位 → 到位后自动掉电
```

**核心收益**：

- 所有编排器可以去掉 `EnsuringEnabled`、`PostEnableDelay`、`EnsuringDisabled`、`PostStopDelay`、`ErrorDisabling` 步骤
- 状态机从 10 步缩减为 4~5 步
- 删除总计约 30 个管理变量和数百行的状态机代码

---

## 三、边沿触发的手动管理

### 3.1 问题描述

PLC 中有多个寄存器采用**边沿触发**协议（上升沿有效），要求上位机先写 ON，等待一定时间后再写 OFF：

| 寄存器           | 用途         |
| ---------------- | ------------ |
| ABS_MOVE_TRIGGER | 触发绝对定位 |
| REL_MOVE_TRIGGER | 触发相对定位 |
| CLEAR_ABS_POS    | 清除绝对位置 |
| SET_REL_ZERO     | 设置相对零点 |
| CLEAR_REL_ZERO   | 清除相对零点 |

### 3.2 对 servoV6 造成的影响

#### 3.2.1 PendingEdge 队列系统

servoV6 不得不在 `ModbusSystemDriver` 中实现一整套边沿触发管理基础设施：

```cpp
struct PendingEdge {
    enum class State { Idle, WroteOn, WroteOff };
    const protocol::RegisterInfo* reg = nullptr;
    State state = State::Idle;
    std::chrono::steady_clock::time_point onTime{};
};
```

配套的队列管理函数多达 **7 个**：

- `enqueueEdge()` / `dequeueEdge()`
- `isEdgePending()` / `pendingEdgeCount()` / `clearPendingEdges()`
- `sendEdgeTrigger()` / `servicePendingEdgeTriggers()`

共计约 **150 行代码**专门用于管理 ON→延迟→OFF 的脉冲时序。

#### 3.2.2 每次 pollFeedback 都要维护边沿队列

`ModbusSystemDriver::pollFeedback()` 的第一行就是：

```cpp
servicePendingEdgeTriggers();  // 扫描队列，超时则写 OFF
```

这个调用耦合在 PLC 反馈轮询的同一帧中，如果 pollFeedback 频率不稳定（如网络波动），边沿脉冲的宽度也会不稳定，可能导致 PLC 无法正确识别触发。

### 3.3 预期接口

PLC 应支持**电平触发**或**写即执行**语义——上位机写 ON 即触发一次操作，PLC 内部自行处理复位。或者更理想地，直接提供**写即执行**的单寄存器接口，上位机只需写一次目标值，PLC 自动执行。

**核心收益**：`PendingEdge` 结构体及其 7 个管理函数（约 150 行代码）可以全部删除。

---

## 四、四寄存器解耦的妥协设计

### 4.1 问题描述

当前 PLC 的绝对/相对定位接口被设计为"参数+触发"分离的两寄存器模型：

- `ABS_TARGET`（D 寄存器，写目标位置）
- `ABS_MOVE_TRIGGER`（M 线圈，边沿触发执行）

这造成一个问题：如果上位机连续下发两次定位，第二次写 `ABS_TARGET` 会覆盖第一次的目标值，但第一次的 `ABS_MOVE_TRIGGER` 触发可能还没执行完。

### 4.2 对 servoV6 造成的影响

servoV6 在领域层引入了 **"四寄存器解耦"**（Phase 1 设计），将原来的 `MoveCommand` 拆分为 4 个独立命令：

```cpp
// 原来：一个 MoveCommand 同时写 target + 触发
struct MoveCommand { MoveType type; double target; double startAbs; };

// 拆分为：
struct SetAbsTargetCommand   { double target; };    // 仅写 ABS_TARGET D 寄存器
struct TriggerAbsMoveCommand {};                     // 仅触发 ABS_MOVE_TRIGGER M 寄存器
struct SetRelTargetCommand   { double distance; };   // 仅写 REL_TARGET D 寄存器
struct TriggerRelMoveCommand {};                     // 仅触发 REL_MOVE_TRIGGER M 寄存器
```

这使得：

- `AxisCommand` variant 从 11 个成员膨胀到 13 个
- `ModbusSystemDriver::send()` 中的 `std::visit` 需要额外处理 4 个 lambda
- 编排器需要在 3~4 步之间协调 target 写入和 trigger 发送的时序

### 4.3 预期接口

PLC 应提供**一个寄存器完成一次操作**的接口（如 `MOVE_ABSOLUTE` 写目标值即触发），或者至少提供**写保护**——当电机正在执行运动时，拒绝写入新的 target，并通过状态位告知上位机"忙"。

**核心收益**：`SetAbsTargetCommand`、`TriggerAbsMoveCommand`、`SetRelTargetCommand`、`TriggerRelMoveCommand` 四个命令可以合并回 `MoveCommand`，简化领域层的命令 variant。

---

## 五、龙门 X 轴接口不一致

### 5.1 问题描述

龙门 X 轴（逻辑轴）控制 X1/X2 物理轴联动。但在 PLC 寄存器布局中，X 轴与 Y/Z/R 轴的接口存在微妙的差异：

- X 轴的状态/命令寄存器分布在 `reg::x_axis::command` / `reg::x_axis::feedback` 命名空间
- X1/X2 物理轴复用 X 轴的寄存器选择器（通过 `[[fallthrough]]` 回退）
- 龙门联动/解耦使用独立寄存器（`LINKAGE_ENABLE`、`GANTRY_ERROR_CODE`）
- X 轴的使能逻辑与单轴不同（JogOrchestrator 中有特殊跳过分支）

### 5.2 对 servoV6 造成的影响

1. **编排器中的特殊分支**：如前述，`JogOrchestrator` 在 `EnsuringDisabled` 步骤对 X 轴有特殊处理
2. **寄存器选择器的 fallthrough**：`ModbusSystemDriver` 的寄存器选择器中大量使用 `[[fallthrough]]`，X/X1/X2 共用同一寄存器地址
3. **龙门状态独立管理**：`GantryCouplingController`、`GantryPowerController` 需要从不同的寄存器读取状态，与其他轴的统一反馈模式不一致
4. **pollFeedback 末尾的龙门状态注入**：在循环完 6 个轴之后，还有一段独立的龙门状态读取和注入逻辑

### 5.3 预期接口

PLC 应将 X 轴（龙门逻辑轴）的接口与单轴（Y/Z/R）统一。龙门联动状态、使能状态、报警代码应通过同一个 `AXIS_STATE_COMBINED` 综合状态寄存器暴露。龙门特有的错误（如 X1/X2 超差）通过 `AXIS_ALARM_CODE` 以独立编码方式区分。

---

## 六、总结：问题影响矩阵

| 问题           | 影响的模块                           | 代码量影响（估算） | 复杂度影响                         |
| -------------- | ------------------------------------ | ------------------ | ---------------------------------- |
| 状态碎片化     | `AxisStateDeriver`、`pollFeedback()` | ~80 行可删除       | 高——5 寄存器融合逻辑               |
| 使能前置依赖   | 3 个 Orchestrator 状态机             | ~300 行可简化      | 极高——每 Orchestrator 10 步→4~5 步 |
| 边沿触发管理   | `PendingEdge` 系统                   | ~150 行可删除      | 中——时序耦合在轮询帧               |
| 四寄存器解耦   | `AxisCommand` variant、`send()`      | ~50 行可简化       | 中——命令变体膨胀                   |
| 龙门接口不一致 | Orchestrator 特殊分支、pollFeedback  | ~30 行可简化       | 低——但易引入回归 bug               |
| 限位二次解析   | `pollFeedback()` 限位判断            | ~30 行可删除       | 低——魔数边界判断                   |

**总计**：当前 servoV6 中约 **500~600 行代码**直接或间接地为弥补 PLC 接口设计缺陷而存在，占 infrastructure 层和 application/policy 层核心逻辑的相当比例。

如果 PLC 层按本文所述的"预期接口"进行改进，servoV6 可以：

1. 删除 `AxisStateDeriver` 及其所有调用点
2. 大幅简化三个 Orchestrator 的状态机
3. 删除 `PendingEdge` 队列系统
4. 合并四寄存器解耦命令
5. 删除限位二次解析的魔数逻辑
6. 统一龙门与单轴的接口模式

从而显著降低代码复杂度、减少 bug 表面积、提升运动响应速度（消除硬编码延迟）。