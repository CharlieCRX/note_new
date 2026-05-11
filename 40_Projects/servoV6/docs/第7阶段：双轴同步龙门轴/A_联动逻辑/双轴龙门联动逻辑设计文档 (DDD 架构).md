这是一个非常扎实的开发思路。在进入代码实现之前，通过这份**自顶向下（Top-Down）的设计文档**，我们可以清晰地定义每一层的职责，确保“用户意图”与“物理现实”在复杂的龙门系统中达到平衡。

------

# 双轴龙门联动逻辑设计文档 (DDD 架构)

## 1. 设计哲学：从“步骤驱动”转向“意图驱动”

- **用户视角：** 只有三个宏观状态（待机、工作、维护）。用户只需关心“我想做什么”。
- **领域视角：** 严格守护物理规则（静止才能耦合、超差必须拦截）。
- **基础设施视角：** 负责具体的寄存器映射（D100 = X1使能，D105 = 联动使能）。

------

## 2. 逻辑架构分层定义

### 第一层：Presentation (表现层)

**组件：** `QtGantryViewModel` & `GantryControlBlock.qml`

- **职责：** * **状态投影：** 将底层的 `GantryMode` 和 `Enable` 状态映射为 UI 的 `Standby/Active/Maintenance` 三态。
  - **权限拦截：** 进入维护模式前触发密码校验。
  - **意图委派：** 接收用户点击，调用 Application 层的“宏指令”。

### 第二层：Application (应用层)

**组件：** `GantryPowerOrchestrator` (新增)

- **职责：** * **原子操作编排：** 实现“宏动作”。例如 `Active -> Standby` 动作包含：`写入解除联动指令` -> `监测解除成功` -> `写入双轴断电指令`。
  - **异常处理：** 如果宏动作中某一步超时或报错，负责回滚或报错。

### 第三层：Domain (领域层) - **系统的核心心脏**

**组件：** `GantrySystem` (聚合根), `GantrySafetyService`

- **职责：** * **状态合法性：** `requestCoupling()` 时检查 `isStandstill` 和 `initialDeviation`。
  - **安全守护：** 周期性监测 `SynchronousDeviation`（同步偏差），一旦超标立刻触发领域事件 `SyncError`。
  - **物理常识：** 维护 `Decoupled` 和 `Coupled` 的状态机。

### 第四层：Infrastructure (基础设施层)

**组件：** `PlcGantryAdapter` (防腐层)

- **职责：** * **语义翻译：** 将 PLC 的 `D105 = 1` 翻译为领域层的 `Coupled` 状态。
  - **物理通讯：** 处理 Modbus/EtherCAT 的读写细节。

------

## 3. 后续开发建议 (基于 TDD)

既然有了这份自顶向下的蓝图，你可以按照以下顺序执行代码：

1. **Domain 层测试：** 编写 `GantrySystemTest.cpp`。测试在 X1 运动时，`requestCoupling` 是否会被拒绝。
2. **Infrastructure 层适配：** 实现 `PlcGantryAdapter`。确保它能把 PLC 的各种报警 Code 正确翻译成你定义的 `GantryException`。
3. **Application 层宏编排：** 实现 `GantryPowerOrchestrator`。这个类不包含具体的 Qt 代码，纯 C++，非常适合 TDD 测试“动作链”的完整性。
4. **UI 层对接：** 最后在 QML 中利用 `States` 和 `Transitions` 配合 `uiState` 枚举实现丝滑的界面裂变效果。

这份文档将作为你项目的“宪法”，无论后续业务如何变动，只要这套分层语义不动，你的项目结构就是稳固的。