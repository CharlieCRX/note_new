------

# 📘 servoV6 UI 架构设计说明文档 (v1.0)

## 一、 设计目标

- **唯一性交互**：UI 必须通过 `AxisViewModel` 操作系统，禁止直接访问 Domain 层。
- **状态投影逻辑**：UI 显示的状态必须 100% 真实反映底层的反馈结果。
- **跨端适配性**：一套代码逻辑同时适配 PAD（高 PPI 触控/物理摇杆）与 PC（中规中矩窗口/键鼠）。
- **组件化扩展**：支持从 Phase 6（单轴）到 Phase 7（多轴联动）的低成本重构。

## 二、 四层原子化 QML 架构

基于“原子设计”理念，我们将 QML 文件划分为四个逻辑层级：

### L0: 核心主题层 (`core/`)

- **职责**：定义全局视觉“基因”，消除硬编码。
- **核心组件**：
  - `Theme.qml`：全局单例。定义工业深色背景、语义状态色（如故障红、就绪绿）、字体尺寸及动态缩放因子（`scaleFactor`）。
  - `qmldir`：注册单例，使全局可用 `Theme.colorError`。

### L1: 基础原子层 (`components/`)

- **职责**：封装纯粹的视觉反馈，无业务逻辑（Stateless）。
- **典型组件**：
  - `IndustrialButton.qml`：处理按下反白、发光特效、触控面积优化，仅通过信号（`pressed/released`）与外界通信。

### L2: 功能模块层 (`blocks/`)

- **职责**：实现具体的业务功能块，作为 **ViewModel 的消费者**。
- **典型组件**：
  - `AxisSelectorBlock.qml`（左侧）：负责轴上下文切换（Context Switcher）。
  - `TelemetryBlock.qml`（中央）：实时数据看板（abs/rel 位置、状态文字）。
  - `ActionControlBlock.qml`（右侧）：运动指令发起区（点动 +/-、绝对定位靶心按钮）。

### L3: 视图总装层 (`views/`)

- **职责**：负责页面布局与“依赖注入”。
- **核心组件**：
  - `MainDashboard.qml`：使用 `RowLayout` 拼装 L2 模块，并持有 `currentAxisVM` 属性，将其分发给各子模块。

## 三、 核心交互逻辑：上下文提供者模式

为了支持多轴切换，UI 采用“指针切换”逻辑：

1. **Context Setter**：左侧 `AxisSelectorBlock` 改变 `MainDashboard` 中的 `currentAxisVM` 指向。
2. **Context Consumer**：中间和右侧模块始终绑定到 `currentAxisVM`。
3. **结果**：点击 X1，全屏显示 X1 数据；点击 X2，界面实时切换为控制 X2，无需刷新页面。

## 四、 状态与命令流向

Plaintext

```
[ 操作输入 ] -> 触发 QtAxisViewModel (Q_INVOKABLE) -> AxisViewModelCore
                                                         │
[ UI 刷新 ] <- 属性绑定 (NOTIFY) <- QtAxisViewModel (tick 脏检查) <- 系统 Tick 驱动
```

- **节流保护**：`QtAxisViewModel` 在 `tick()` 中进行位置数值对比，只有变化超过阈值（如 0.001）才发射 `Changed` 信号，防止 UI 渲染过载。

## 五、 跨端适配策略

- **尺寸适配**：所有尺寸基于 `Theme.scale` 动态计算。
- **输入适配**：
  - **PAD**：依赖 `onPressed/onReleased` 进行点动控制。
  - **PC**：在 `Main.qml` 注入全局 `Keys` 拦截，将方向键映射给 `jogPositivePressed`，空格键映射给 `stop`。

------

### 下一步建议

目前目录结构已就绪，建议按以下顺序填充代码：

1. **L0 注入**：完成 `Theme.qml` 定义。
2. **L1 封装**：实现 `IndustrialButton.qml`（这是 UI 的手感来源）。
3. **L3/L2 联调**：在 `MainDashboard` 中跑通第一个 X1 轴的控制链路。

