------

# 🚀 servoV6 开发规范（C++能力强化版 v2.0）

> 关键词：
> **C++训练场 + QtQuick UI + GoogleTest + TDD + Clean Architecture**

------

# 🧠 一、核心升级（你必须理解）

这一版的核心变化只有一句话：

```text
从“写代码” → “训练工程能力系统”
```

### 升级点：

| 维度    | v1       | v2（现在）                  |
| ------- | -------- | --------------------------- |
| 架构    | 5层      | ✅ 6层（+DomainService）     |
| C++训练 | STL      | ✅ STL + RAII + value object |
| 测试    | 有测试   | ✅ TDD驱动开发               |
| UI      | Qt       | ✅ QtQuick + ViewModel       |
| 抽象    | 基本分层 | ✅ HAL + Driver隔离          |

------

# 🏗 二、最终架构（强制执行）

```text
UI（Qt/QML）
↓
Presentation（ViewModel）
↓
Application（UseCase）
↓
Domain（实体/值对象）
↓
Domain Service（规则/算法）
↓
Infrastructure（通信/驱动）
↓
HAL（硬件抽象）
```

👉 核心新增：

```text
Domain Service = 复杂逻辑归属地
HAL = 可测试 + 可替换硬件
```

------

# 🔒 三、C++强化核心规则（Domain层 = 训练核心）

> 这一部分必须“严格执行”，否则你不会成长

------

## 3.1 内存与对象（强制RAII）

❌ 禁止：

```cpp
new / delete
裸指针所有权
```

✅ 必须：

```cpp
std::unique_ptr
std::shared_ptr
std::optional
```

👉 训练目标：

```text
理解生命周期 + 所有权模型
```

------

## 3.2 值对象（重点升级🔥）

你之前没有这个，这是本次最大升级点

### ❌ 错误写法：

```cpp
double position;
double speed;
```

### ✅ 正确写法：

```cpp
class Position {
public:
    explicit Position(double value);
    double value() const;

private:
    double value_;
};
```

👉 训练：

```text
封装 + 不变量 + 类型安全
```

------

## 3.3 状态建模（禁止 bool）

❌ 禁止：

```cpp
bool moving;
bool alarm;
```

✅ 必须：

```cpp
enum class AxisState {
    Idle,
    Moving,
    Error
};
```

👉 训练：

```text
状态机思维（工业软件核心）
```

------

## 3.4 STL 强制使用（升级版）

### 每周必须使用：

```cpp
std::transform
std::ranges::views (C++20)
std::find_if
std::max_element
```

👉 禁止：

```cpp
for + if 堆逻辑
```

👉 目标：

```text
从“会用C++” → “写现代C++”
```

------

## 3.5 Domain 绝对纯净

禁止一切副作用：

```text
❌ 日志
❌ IO
❌ 线程
❌ Qt
❌ Modbus
```

👉 原因：

```text
保证100%可测试（TDD基础）
```

------

# 🧩 四、Domain Service（新增核心规则🔥）

> 你之前最大问题：逻辑写在 UseCase

------

## 什么必须进 DomainService？

### ✅ 放这里：

```text
多轴同步
运动规划
误差计算
报警判断
```

### ❌ 不放：

```text
单对象行为（留在Entity）
流程控制（留在UseCase）
```

------

## 示例（你项目核心）

```cpp
class GantrySyncChecker {
public:
    bool isOutOfTolerance(Position x1, Position x2);
};
```

👉 这就是：

```text
X1X2 超差判断正确归属
```

------

# 🔄 五、Application（UseCase）规范（TDD重点）

------

## 核心职责：

```text
编排流程（不是写业务逻辑）
```

------

## 标准结构（必须统一）

```cpp
void JogAxisUseCase::execute(AxisId id, Direction dir)
{
    auto axis = repository_.get(id);

    if (!axis.canJog()) {
        return;
    }

    auto cmd = axis.createJogCommand(dir);

    driver_.send(cmd);
}
```

------

## ❌ 禁止：

```text
写算法
写寄存器
写Qt
```

------

## 👉 训练目标：

```text
流程拆解能力（架构核心能力）
```

------

# 🧪 六、TDD开发规范（GoogleTest）

> 这是你能力跃迁的关键

------

## 开发顺序（强制🔥）

```text
1 写测试（失败）
2 写最小实现
3 重构
```

------

## 示例（Domain）

```cpp
TEST(AxisTest, ShouldEnterMovingStateAfterJog)
{
    Axis axis;

    axis.jog(Direction::Forward);

    EXPECT_EQ(axis.state(), AxisState::Moving);
}
```

------

## 覆盖范围（必须）

```text
Domain：100%
UseCase：核心流程
DomainService：算法
```

------

## ❌ 不测：

```text
Qt UI
```

------

# 🖥 七、QtQuick + ViewModel 规范（重点升级）

------

## UI 只能做：

```text
展示 + 用户输入
```

------

## ViewModel 是关键：

```text
QML ↔ UseCase 桥梁
```

------

## 示例：

```cpp
void AxisViewModel::onJogClicked()
{
    usecase_.execute(axisId_, Direction::Forward);
}
```

------

## ❌ 禁止：

```text
UI直接调用驱动
UI写业务逻辑
```

------

# 🔌 八、HAL + Driver（工程能力核心）

------

## 强制接口：

```cpp
class IAxisDriver {
public:
    virtual void send(const Command&) = 0;
};
```

------

## 实现：

```text
PlcAxisDriver（真实）
FakeAxisDriver（测试）
```

------

## 👉 价值：

```text
脱离PLC开发（效率提升核心）:contentReference[oaicite:4]{index=4}
```

------

# 📏 九、复杂度与代码纪律

------

## 强制：

```text
函数 < 40行
类 < 300行
```

------

## 命名规范（结合你习惯）

```cpp
row_start_ptr
line_start_ptr
axis_state
```

👉 保持：

```text
语义清晰 + 同一抽象层
```

------

# 📈 十、成长驱动规则（最重要🔥）

------

## 每周必须：

### 1️⃣ STL训练

```text
至少3个算法
```

------

### 2️⃣ 重构

```text
至少1个模块
```

------

### 3️⃣ TDD

```text
≥ 2个测试
```

------

### 4️⃣ 架构检查（关键）

```text
Domain是否污染？
UseCase是否写逻辑？
```

------

# 🧭 十一、你这个项目真正的目标（我帮你拉高一层）

不是：

```text
写一个控制软件
```

而是：

```text
打造一个“工业级C++能力训练系统”
```

------

# ⚠️ 最后我给你一个“狠一点的约束”（非常关键）

你必须执行这条：

```text
任何 if > 3层
→ 必须重构为：
    状态机 / 多态 / 策略模式
```

------

# 🎯 我建议你下一步（非常关键）

我们可以直接进入一个**实战训练模式**👇：

👉 我带你一步一步：

### 第一轮（强烈建议现在做）

```text
实现：
Axis + Jog + 状态机

要求：
1 Domain纯净
2 TDD驱动
3 不接PLC
```

我会：

- 带你写测试
- 带你拆类
- 帮你纠错（这是关键）

------

如果你愿意，我们下一步直接做：

👉 **“第一版 Axis（带状态机）TDD实现”**

这一步，会直接决定你后面是否真正掌握这套架构。