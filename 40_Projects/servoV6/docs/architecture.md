------

# 一、servoV6 项目总纲（升级版）

你当前的架构已经是对的（而且很高级），核心是：

> Clean Architecture + 工业控制建模 + TDD

但还不够“工程化”。

我帮你升级为 **6层结构（关键升级点）**：

------

## 🧱 1. 最终推荐架构（重要）

```text
UI（Qt/QML）
↓
Presentation（ViewModel）
↓
Application（UseCase）
↓
Domain（核心模型）
↓
Domain Service（领域服务） ⭐新增
↓
Infrastructure（通信/驱动）
```

------

## ⚠️ 为什么要加 Domain Service？

你现在的风险是：

👉 UseCase 变“上帝类”
👉 Domain 变“贫血模型”

所以必须拆出：

```text
Domain：状态 + 行为（对象内）
DomainService：跨对象逻辑（规则/算法）
```

------

# 二、项目模块大纲（真正能开发的结构）

我按“可落地目录”给你：

------

## 📂 1. UI 层（Qt/QML）

```text
ui/
├── pages/
│   ├── AxisPage.qml
│   ├── GantryPage.qml
│
├── components/
│   ├── AxisControlPanel.qml
│   ├── AlarmIndicator.qml
```

👉 对应你的需求：单轴 / X1X2 / 报警显示

------

## 📂 2. Presentation（ViewModel）

```text
presentation/
├── AxisViewModel
├── GantryViewModel
├── SystemViewModel
```

职责：

```text
UI → UseCase
Domain → UI数据转换
```

------

## 📂 3. Application（UseCase）

```text
application/
├── axis/
│   ├── JogAxisUseCase
│   ├── MoveAbsoluteUseCase
│   ├── MoveRelativeUseCase
│   ├── StopAxisUseCase
│
├── system/
│   ├── ConnectPlcUseCase
│   ├── ResetAlarmUseCase
```

核心特点：

```text
流程控制（不是业务逻辑）
```

------

## 📂 4. Domain（核心模型）

```text
domain/
├── entity/
│   ├── Axis
│   ├── GantryAxis（X1X2）
│   ├── Motor
│
├── value/
│   ├── Position
│   ├── Speed
│   ├── AxisState
│
├── command/
│   ├── MoveCommand
│   ├── JogCommand
```

👉 这里是你C++能力提升核心区

------

## 📂 5. ⭐ Domain Service（新增核心）

```text
domain/service/
├── AxisMotionPlanner
├── GantrySyncChecker
├── AlarmEvaluator
```

### 举例：

```cpp
class GantrySyncChecker
{
public:
    bool isOutOfTolerance(double x1, double x2, double limit);
};
```

👉 这就是你 X1X2 “超差报警”的正确归属

------

## 📂 6. Infrastructure

```text
infrastructure/
├── transport/
│   ├── ModbusTcpTransport
│
├── gateway/
│   ├── AxisGateway
│
├── mapping/
│   ├── RegisterMap
```

👉 所有 PLC 地址都只能在这里

------

## 📂 7. HAL（你提出的重点）

这是你可以“加分”的地方👇

```text
hal/
├── IAxisDriver
├── PlcAxisDriver
├── SimulatedAxisDriver（测试）
```

👉 价值：

```text
UI / UseCase 完全不关心 PLC
未来可接 ARM / EtherCAT
```

------

## 📂 8. Tests（必须重点做）

```text
tests/
├── domain/
├── application/
```

------

# 三、核心数据流

## 控制流：

```text
UI
→ ViewModel
→ UseCase
→ Domain
→ DomainService
→ Gateway
→ PLC
```

------

## 状态回读：

```text
PLC
→ Transport
→ Gateway
→ Domain（更新状态）
→ ViewModel
→ UI
```

------

