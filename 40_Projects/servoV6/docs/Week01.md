------

# 🗓 第一周目标

```text
完成系统架构落地，构建一个“不依赖PLC”的可运行最小系统，
并验证核心数据流（UI → Domain → Driver）
```

------

# 🎯 核心原则（这一周必须死守）

```text
1. 不接PLC（禁止）
2. 不写寄存器（禁止）
3. 必须能跑（必须）
4. 必须有测试（必须）
```

👉 否则这一周直接失败

------

# 📆 一周详细拆解（每天做什么）

------

# 🚀 Day1：项目骨架 + 编译通过

------

## 🎯 目标

```text
搭建完整分层结构（空壳也行）
```

------

## 📦 工作内容

### 1️⃣ 创建目录结构

```bash
servoV6/
├── ui/
├── presentation/
├── application/
├── domain/
├── domain_service/
├── infrastructure/
├── hal/
├── tests/
```

------

### 2️⃣ 建立基础接口（必须）

```cpp
// hal/IAxisDriver.h
class IAxisDriver {
public:
    virtual void send(const Command&) = 0;
};
```

------

### 3️⃣ 建立最小Domain模型

```cpp
// domain/Axis.h
class Axis {
public:
    void jogPositive();
};
```

------

### 4️⃣ CMake / 编译通过

------

## ✅ 当天验收

```text
✔ 项目可以编译
✔ 所有模块可引用
✔ 没有循环依赖
```

------

## ⚠️ 常见错误

```text
❌ Domain include Qt
❌ Application include Modbus
```

------

# 🚀 Day2：Domain建模（核心）

------

## 🎯 目标

```text
完成 Axis 的“最小可用模型”
```

------

## 📦 工作内容

### 1️⃣ Axis状态设计

```cpp
enum class AxisState {
    Idle,
    Moving,
    Error
};
```

------

### 2️⃣ Axis行为

```cpp
void Axis::jogPositive()
{
    if (state_ == AxisState::Error)
        return;

    state_ = AxisState::Moving;
    direction_ = Direction::Positive;
}
```

------

### 3️⃣ Command建模（关键）

```cpp
enum class CommandType {
    JogPositive
};

struct Command {
    CommandType type;
};
```

------

### 4️⃣ Axis生成命令

```cpp
Command Axis::command() const;
```

------

## ✅ 当天验收

```text
✔ Axis可以独立运行（main函数测试）
✔ jogPositive 后状态变化正确
✔ command生成正确
```

------

## 🎯 关键评估点（你可以说）

```text
业务逻辑已经脱离硬件存在
```

------

# 🚀 Day3：UseCase + FakeDriver

------

## 🎯 目标

```text
打通“业务流程”
```

------

## 📦 工作内容

### 1️⃣ JogAxisUseCase

```cpp
class JogAxisUseCase {
public:
    void execute(int axisId);
};
```

------

### 2️⃣ FakeDriver实现

```cpp
class FakeAxisDriver : public IAxisDriver {
public:
    void send(const Command& cmd) override
    {
        std::cout << "Fake send JogPositive\n";
    }
};
```

------

### 3️⃣ UseCase调用链

```cpp
axis.jogPositive();
driver.send(axis.command());
```

------

## ✅ 当天验收

```text
✔ 调用UseCase → FakeDriver输出日志
✔ 没有PLC也能运行
```

------

## 🎯 关键评估点

```text
流程已跑通（UI除外）
```

------

# 🚀 Day4：Presentation + UI最小联通

------

## 🎯 目标

```text
UI → UseCase 打通
```

------

## 📦 工作内容

### 1️⃣ ViewModel

```cpp
void AxisViewModel::jogPositive()
{
    usecase.execute(axisId);
}
```

------

### 2️⃣ UI按钮

```qml
Button {
    text: "点动+"
    onClicked: axisVM.jogPositive()
}
```

------

## ✅ 当天验收

```text
✔ 点击按钮 → FakeDriver输出日志
✔ 无PLC运行
```

------

## 🎯 关键评估点

```text
完整链路首次打通
```

------

# 🚀 Day5：单元测试（必须）

------

## 🎯 目标

```text
建立测试体系（非常重要）
```

------

## 📦 工作内容

### 1️⃣ Domain测试

```cpp
TEST(AxisTest, JogPositive)
{
    Axis axis;
    axis.jogPositive();

    ASSERT_EQ(axis.state(), AxisState::Moving);
}
```

------

### 2️⃣ UseCase测试（可选）

------

## ✅ 当天验收

```text
✔ 至少3个测试用例
✔ 全部通过
```

------

## 🎯 关键评估点

```text
系统具备“可验证能力”
```

------

# 🚀 Day6：重构 + 清理

------

## 🎯 目标

```text
让结构“干净”
```

------

## 📦 工作内容

```text
✔ 删除重复代码
✔ 拆分函数（<40行）
✔ 调整命名
✔ 检查依赖方向
```

------

## ✅ 验收

```text
✔ 每层职责清晰
✔ 没有跨层调用
```

------

# 🚀 Day7：总结 + 文档 + 演示

------

## 🎯 目标

```text
让领导“看懂”
```

------

## 📦 输出内容

### 1️⃣ 架构图

```text
UI → ViewModel → UseCase → Domain → Driver
```

------

### 2️⃣ Demo演示

```text
点击按钮 → 控制台输出
```

------

### 3️⃣ 一页总结

```text
已完成：
✔ 分层架构落地
✔ 核心数据流打通
✔ 可脱离PLC开发
```

------

# 📊 最终验收标准（你必须背）

------

## 🎯 功能层面

```text
✔ UI点击 → 能触发点动流程
✔ FakeDriver正确输出
```

------

## 🎯 架构层面（重点）

```text
✔ Domain无PLC代码
✔ UseCase无寄存器
✔ Driver隔离硬件
```

------

## 🎯 工程层面

```text
✔ 项目可编译
✔ 有单元测试
✔ 可运行
```

------

# ❌ 第一周失败的典型表现（你要避免）

------

```text
❌ 写了Modbus代码
❌ 直接操作寄存器
❌ 没有测试
❌ UI写业务逻辑
```

------

# 🧠 最后一句（你汇报可以用）

------

```text
第一周的重点不是功能，而是：

建立一个“可运行、可测试、可扩展”的架构基础，

为后续PLC接入和多轴扩展提供稳定基础。
```