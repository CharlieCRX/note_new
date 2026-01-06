# TDD开发流程

以业务目标为出发点，从行为驱动接口，再逐步落地实现。

TDD开发流程：

1. 明确需求，编写测试用例（Red 阶段）
2. 编写最小化代码，使其通过测试（Green 阶段）
3. 重构代码（Refactor 阶段）

## 目标1：相对位置移动

### 1. 明确需求与编写测试用例 (Red 阶段)

首先，我们根据 "设置速度20mm/s → 相对位置移动上升/下降 30mm → 停顿 2s → 回原点" 这个业务需求，编写一个会失败的测试用例。

现在的目标业务行为是：

> "设置速度20mm/s → 相对位置移动上升/下降 30mm → 停顿 2s → 回原点"

可以抽象为一个序列化/配置化的命令结构，例如：

```cpp
CommandSequence commands;
commands.push_back(SetSpeed{20.0});        // 设置速度 20mm/s
commands.push_back(RelativeMove{30.0});    // 相对移动 30mm
commands.push_back(Wait{2000});            // 停顿 2000ms
commands.push_back(GoHome{});              // 回原点
```

#### 准备 Mock 对象

创建象接口类`IMotor`，它定义了电机控制层对外提供的一组行为方法（如设置速度、移动、停顿、回原点），但不包含具体实现。具体的电机控制逻辑会由实现该接口的具体类（比如 `Motor` 或 `MockMotor`）来完成。

- **抽象电机的操作行为**，屏蔽底层硬件细节。
- **方便单元测试**，用模拟类（`MockMotor`）替代硬件实现，验证业务逻辑调用正确。
- **解耦业务逻辑与硬件驱动**，使业务逻辑层只依赖接口，不依赖具体实现。

在目标1需求下的示例接口：

```cpp
// core/IMotor.h
#ifndef IMOTOR_H
#define IMOTOR_H

class IMotor {
public:
    virtual ~IMotor() = default; // Virtual destructor to ensure proper cleanup of derived objects

    virtual bool setSpeed(double mmPerSec) = 0;    // Pure virtual function: Set speed, returns true on success
    virtual bool relativeMove(double mm) = 0;      // Pure virtual function: Relative move, returns true on success
    virtual void wait(int ms) = 0;                 // Pure virtual function: Pause (wait)
    virtual bool goHome() = 0;                     // Pure virtual function: Go to home position, returns true on success
};

#endif // IMOTOR_H
```

- 其中 `=0` 表示这是纯虚函数，必须由派生类实现。
- `IMotor` 不能直接实例化，只能通过继承实现。

现在，创建 `MockMotor`

```cpp
// tests/mocks/MockMotor.h
#ifndef MOCK_MOTOR_H
#define MOCK_MOTOR_H

#include <gmock/gmock.h>
#include "IMotor.h" // 包含真实的 IMotor 接口

class MockMotor : public IMotor {
public:
    // 使用 MOCK_METHOD 定义虚函数的 Mock
    MOCK_METHOD(bool, setSpeed, (double mmPerSecond), (override));
    MOCK_METHOD(bool, relativeMove, (double mm), (override));
    MOCK_METHOD(void, wait, (int ms), (override));
    MOCK_METHOD(bool, goHome, (), (override));
};

#endif // MOCK_MOTOR_H
```

#### 编写 `MovementCommand` 结构体

```cpp
// core/MovementCommand.h
#ifndef MOVEMENT_COMMAND_H
#define MOVEMENT_COMMAND_H

#include <variant>
#include <vector>

// 定义各种命令的结构体
struct SetSpeed {
    double mm_per_sec;
};

struct RelativeMove {
    double delta_mm;
};

struct Wait {
    long milliseconds;
};

struct GoHome {};  // 无参数

// 使用 std::variant 定义 Command 类型
using Command = std::variant<SetSpeed, RelativeMove, Wait, GoHome>;

// 定义命令序列
using CommandSequence = std::vector<Command>;

#endif // MOVEMENT_COMMAND_H
```

#### `core/BusinessLogic.h` (定义业务逻辑接口)

```cpp
// core/BusinessLogic.h
#ifndef BUSINESS_LOGIC_H
#define BUSINESS_LOGIC_H

#include "IMotor.h"
#include "MovementCommand.h"
#include <map>
#include <string>
#include <vector>
#include <memory> // For std::unique_ptr

class BusinessLogic {
public:
    // 构造函数接收一个电机ID到IMotor实例的映射
    BusinessLogic(std::map<std::string, std::unique_ptr<IMotor>> motors);
    ~BusinessLogic();

    // 核心业务方法：执行指定电机的命令序列
    bool executeCommandSequence(const std::string& motorId, const CommandSequence& commands);

private:
    std::map<std::string, std::unique_ptr<IMotor>> motorMap;
};

#endif // BUSINESS_LOGIC_H
```

#### `tests/test_businesslogic.cpp` (核心测试用例 - **Red** 阶段)

```cpp
// tests/test_businesslogic.cpp
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "core/BusinessLogic.h"
#include "core/MovementCommand.h"
#include "tests/mocks/MockMotor.h" // 确保路径正确

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

class BusinessLogicTest : public ::testing::Test {
protected:
    MockMotor* mockMotorRawPtr; // 裸指针，用于设置期望
    std::unique_ptr<BusinessLogic> businessLogic; // 业务逻辑实例

    void SetUp() override {
        auto mockMotorUniquePtr = std::make_unique<MockMotor>();
        mockMotorRawPtr = mockMotorUniquePtr.get();

        std::map<std::string, std::unique_ptr<IMotor>> motors;
        motors["main_motor"] = std::move(mockMotorUniquePtr);

        businessLogic = std::make_unique<BusinessLogic>(std::move(motors));
    }

    void TearDown() override {
        // unique_ptr 会自动管理内存
    }
};

TEST_F(BusinessLogicTest, ExecutesComplexMovementSequence) {
    // 预期行为：设置速度 -> 相对移动 -> 停顿 -> 回原点
    InSequence s;

    // 1. 期望 setSpeed(20.0) 被调用一次，并返回 true
    EXPECT_CALL(*mockMotorRawPtr, setSpeed(20.0)).WillOnce(Return(true));

    // 2. 期望 relativeMove(30.0) 被调用一次，并返回 true
    EXPECT_CALL(*mockMotorRawPtr, relativeMove(30.0)).WillOnce(Return(true));

    // 3. 期望 wait(2000) 被调用一次
    EXPECT_CALL(*mockMotorRawPtr, wait(2000));

    // 4. 期望 goHome() 被调用一次，并返回 true
    EXPECT_CALL(*mockMotorRawPtr, goHome()).WillOnce(Return(true));

    // 准备命令序列
    CommandSequence commands;
    commands.push_back(SetSpeed{20.0});       // 设置速度 20mm/s
    commands.push_back(RelativeMove{30.0});   // 相对移动 30mm
    commands.push_back(Wait{2000});           // 停顿 2000ms
    commands.push_back(GoHome{});             // 回原点

    // 调用被测试的业务逻辑方法
    bool result = businessLogic->executeCommandSequence("main_motor", commands);

    // 断言整个命令序列执行成功
    ASSERT_TRUE(result);
}

// 考虑添加一个测试，测试当某个电机操作失败时 BusinessLogic 的行为
TEST_F(BusinessLogicTest, ReturnsFalseOnMotorOperationFailure) {
    InSequence s;

    EXPECT_CALL(*mockMotorRawPtr, setSpeed(20.0)).WillOnce(Return(true));
    // 模拟 relativeMove 失败
    EXPECT_CALL(*mockMotorRawPtr, relativeMove(30.0)).WillOnce(Return(false));
    // 期望后续的 wait 和 goHome 不会被调用
    EXPECT_CALL(*mockMotorRawPtr, wait(_)).Times(0);
    EXPECT_CALL(*mockMotorRawPtr, goHome()).Times(0);

    CommandSequence commands;
    commands.push_back(SetSpeed{20.0});
    commands.push_back(RelativeMove{30.0});
    commands.push_back(Wait{2000});
    commands.push_back(GoHome{});

    bool result = businessLogic->executeCommandSequence("main_motor", commands);

    // 断言执行失败
    ASSERT_FALSE(result);
}

// 针对 MovementCommand 中有 Stop 但 IMotor 没有 stop() 的情况，添加测试
TEST_F(BusinessLogicTest, HandlesUnsupportedStopCommand) {
    // 期望 setSpeed 成功，然后遇到 Stop 命令，BusinessLogic 应该如何处理？
    // 1. 忽略 Stop 命令并继续（如果后续有命令）
    // 2. 视为错误并返回 false
    // 这里的测试决定了 BusinessLogic 的行为。我们假设它应该返回 false
    InSequence s;
    EXPECT_CALL(*mockMotorRawPtr, setSpeed(10.0)).WillOnce(Return(true));
    // 不对 mockMotorRawPtr 调用 stop() 设置期望，因为它不存在

    CommandSequence commands;
    commands.push_back(SetSpeed{10.0});
    commands.push_back(Stop{}); // 添加 Stop 命令

    bool result = businessLogic->executeCommandSequence("main_motor", commands);

    // 期望遇到不支持的命令时返回 false
    ASSERT_FALSE(result);
}
```

### 2.编写最小化代码使其通过测试 (Green 阶段)

实现 `BusinessLogic::executeCommandSequence` 方法，以使上述测试通过。

```cpp
// core/BusinessLogic.cpp
#include "BusinessLogic.h"
#include <variant>     // 用于 std::visit
#include <Logger.h>

BusinessLogic::BusinessLogic(std::map<std::string, std::unique_ptr<IMotor>> motors)
    : motorMap(std::move(motors)) {
    // 构造函数中可以添加日志，表明业务逻辑实例被创建
    LOG_DEBUG("BusinessLogic instance created with {} motors.", motorMap.size());
}

BusinessLogic::~BusinessLogic() {
    // unique_ptr 会自动清理
    LOG_DEBUG("BusinessLogic instance destroyed.");
}

bool BusinessLogic::executeCommandSequence(const std::string& motorId, const CommandSequence& commands) {
    auto it = motorMap.find(motorId);
    if (it == motorMap.end()) {
        LOG_ERROR("Motor '{}' not found for command sequence execution.", motorId);
        return false;
    }

    IMotor* motor = it->second.get();
    bool success = true; // 用于跟踪命令执行是否成功

    LOG_INFO("Executing command sequence for motor '{}' ({} commands).", motorId, commands.size());

    for (const auto& cmd : commands) {
        // 使用 std::visit 处理不同的命令类型
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, SetSpeed>) {

                LOG_DEBUG("Setting speed for motor '{}' to {} mm/s", motorId, arg.mm_per_sec);

                if (!motor->setSpeed(arg.mm_per_sec)) {
                    LOG_ERROR("Failed to set speed for motor '{}'.", motorId);
                    success = false;
                }
            } else if constexpr (std::is_same_v<T, RelativeMove>) {
                LOG_DEBUG("Moving motor '{}' by {} mm", motorId, arg.delta_mm);
                if (!motor->relativeMove(arg.delta_mm)) {

                    LOG_ERROR("Failed to move motor '{}'.", motorId);
                    success = false;
                }
            } else if constexpr (std::is_same_v<T, Wait>) {

                LOG_DEBUG("Waiting for {} ms...", arg.milliseconds);
                motor->wait(arg.milliseconds); // IMotor 已经提供了 wait 方法
            } else if constexpr (std::is_same_v<T, GoHome>) {

                LOG_DEBUG("Homing motor '{}'...", motorId);
                if (!motor->goHome()) {

                    LOG_ERROR("Failed to home motor '{}'.", motorId);
                    success = false;
                }
            }
        }, cmd);

        // 如果在处理当前命令时发生了错误，则立即停止并返回 false
        if (!success) {
            LOG_WARN("Command execution failed for motor '{}'. Aborting sequence.", motorId);
            return false;
        }
    }
    LOG_INFO("Command sequence executed successfully for motor '{}'.", motorId);
    return true; // 所有命令都成功执行
}
```



## 目标2：绝对位置移动

业务行为是让电机移动到**绝对位置**。这意味着电机将移动到一个特定的坐标点，而不是相对于当前位置移动。

您好！非常棒的 TDD 实践。我们已经完成了“相对位置移动”需求的第一轮 Red-Green-Refactor 循环（尽管重构阶段还没有详细展开，但代码已经很清晰了）。

现在，我们按照 TDD 的流程，继续实现 **目标2：绝对位置移动**。

------



## 目标2：绝对位置移动

### 1. 明确需求与编写测试用例 (Red 阶段)

针对“绝对位置移动”的需求，我们需要定义一个新的命令类型，并编写一个测试用例，该测试用例在执行时会失败（因为我们还没有实现它）。

#### 需求描述

新的业务行为是让电机移动到**绝对位置**。这意味着电机将移动到一个特定的坐标点，而不是相对于当前位置移动。

#### 抽象为命令结构

我们需要一个新的命令结构来表示绝对位置移动：

```C++
// core/MovementCommand.h (新增 AbsoluteMove 结构体)
struct AbsoluteMove {
    double target_mm; // 目标绝对位置（毫米）
};

// 并且将它添加到 Command std::variant 中
using Command = std::variant<SetSpeed, RelativeMove, Wait, GoHome, AbsoluteMove>;
```

#### 修改 `IMotor` 接口

为了支持绝对位置移动，`IMotor` 接口需要添加一个纯虚函数。

```C++
// core/IMotor.h (新增 absoluteMove 函数)
#ifndef IMOTOR_H
#define IMOTOR_H

class IMotor {
public:
    virtual ~IMotor() = default;

    virtual bool setSpeed(double mmPerSec) = 0;
    virtual bool relativeMove(double mm) = 0;
    virtual void wait(int ms) = 0;
    virtual bool goHome() = 0;
    virtual bool absoluteMove(double targetMm) = 0; // <-- 新增的纯虚函数
};

#endif // IMOTOR_H
```

#### 修改 `MockMotor`

由于 `IMotor` 接口改变了，`MockMotor` 也需要同步更新，添加对 `absoluteMove` 的 Mock。

```C++
// tests/mocks/MockMotor.h (新增 absoluteMove 的 Mock)
#ifndef MOCK_MOTOR_H
#define MOCK_MOTOR_H

#include <gmock/gmock.h>
#include "IMotor.h"

class MockMotor : public IMotor {
public:
    MOCK_METHOD(bool, setSpeed, (double mmPerSecond), (override));
    MOCK_METHOD(bool, relativeMove, (double mm), (override));
    MOCK_METHOD(void, wait, (int ms), (override));
    MOCK_METHOD(bool, goHome, (), (override));
    MOCK_METHOD(bool, absoluteMove, (double targetMm), (override)); // <-- 新增的 Mock
};

#endif // MOCK_MOTOR_H
```

#### 编写测试用例 (`tests/test_businesslogic.cpp`)

我们将添加一个新的测试用例，它将包含 `AbsoluteMove` 命令。在 `BusinessLogic::executeCommandSequence` 未实现对 `AbsoluteMove` 的处理之前，这个测试用例会失败（Red）。

```C++
// tests/test_businesslogic.cpp (新增 ExecutesAbsoluteMovement 测试)
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "core/BusinessLogic.h"
#include "core/MovementCommand.h"
#include "tests/mocks/MockMotor.h"
#include "core/Logger.h" // 确保包含 Logger.h 以使用 LOG_宏

// ... (BusinessLogicTest 夹具保持不变，因为它已经处理了 MockMotor 的设置) ...

// 继续在 BusinessLogicTest 夹具中添加新的 TEST_F
TEST_F(BusinessLogicTest, ExecutesAbsoluteMovement) {
    // 预期行为：设置速度 -> 绝对移动 -> 回原点
    InSequence s;

    // 1. 期望 setSpeed(10.0) 被调用一次，并返回 true
    EXPECT_CALL(*mockMotorRawPtr, setSpeed(10.0)).WillOnce(Return(true));

    // 2. 期望 absoluteMove(50.0) 被调用一次，并返回 true
    // 在 BusinessLogic 中尚未处理 AbsoluteMove 时，这个 EXPECT_CALL 将不会被触发
    // 或者，如果 BusinessLogic::executeCommandSequence 遇到未处理的 variant 变体，它可能会崩溃或返回 false
    EXPECT_CALL(*mockMotorRawPtr, absoluteMove(50.0)).WillOnce(Return(true));

    // 3. 期望 goHome() 被调用一次，并返回 true
    EXPECT_CALL(*mockMotorRawPtr, goHome()).WillOnce(Return(true));

    // 准备命令序列，包含新的 AbsoluteMove 命令
    CommandSequence commands;
    commands.push_back(SetSpeed{10.0});
    commands.push_back(AbsoluteMove{50.0}); // 移动到绝对位置 50mm
    commands.push_back(GoHome{});

    // 调用被测试的业务逻辑方法
    bool result = businessLogic->executeCommandSequence("main_motor", commands);

    // 断言整个命令序列执行成功
    ASSERT_TRUE(result);
    LOG_INFO("Test 'ExecutesAbsoluteMovement' finished successfully.");
}

// ... (其他测试用例保持不变) ...
```

------

### 2. 编写最小化代码使其通过测试 (Green 阶段)

现在，我们将修改 `BusinessLogic::executeCommandSequence` 方法，以处理新的 `AbsoluteMove` 命令，并使其通过 `ExecutesAbsoluteMovement` 测试。

#### 修改 `core/BusinessLogic.cpp`

```C++
// core/BusinessLogic.cpp (更新 executeCommandSequence 以处理 AbsoluteMove)
#include "BusinessLogic.h"
#include <variant>
#include "core/Logger.h" // 确保包含 Logger.h

// ... (构造函数和析构函数保持不变) ...

bool BusinessLogic::executeCommandSequence(const std::string& motorId, const CommandSequence& commands) {
    auto it = motorMap.find(motorId);
    if (it == motorMap.end()) {
        LOG_ERROR("Motor '{}' not found for command sequence execution.", motorId);
        return false;
    }

    IMotor* motor = it->second.get();
    bool success = true;

    LOG_INFO("Executing command sequence for motor '{}' ({} commands).", motorId, commands.size());

    for (const auto& cmd : commands) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, SetSpeed>) {
                LOG_DEBUG("Setting speed for motor '{}' to {} mm/s.", motorId, arg.mm_per_sec);
                if (!motor->setSpeed(arg.mm_per_sec)) {
                    LOG_ERROR("Failed to set speed for motor '{}'.", motorId);
                    success = false;
                }
            } else if constexpr (std::is_same_v<T, RelativeMove>) {
                LOG_DEBUG("Moving motor '{}' by {} mm.", motorId, arg.delta_mm);
                if (!motor->relativeMove(arg.delta_mm)) {
                    LOG_ERROR("Failed to move motor '{}'.", motorId);
                    success = false;
                }
            } else if constexpr (std::is_same_v<T, Wait>) {
                LOG_DEBUG("Waiting for {} ms...", arg.milliseconds);
                motor->wait(arg.milliseconds);
            } else if constexpr (std::is_same_v<T, GoHome>) {
                LOG_DEBUG("Homing motor '{}'...", motorId);
                if (!motor->goHome()) {
                    LOG_ERROR("Failed to home motor '{}'.", motorId);
                    success = false;
                }
            } else if constexpr (std::is_same_v<T, AbsoluteMove>) { // <-- 新增的 AbsoluteMove 处理
                LOG_DEBUG("Moving motor '{}' to absolute position {} mm.", motorId, arg.target_mm);
                if (!motor->absoluteMove(arg.target_mm)) {
                    LOG_ERROR("Failed to move motor '{}' to absolute position {}.", motorId, arg.target_mm);
                    success = false;
                }
            } else { // <-- 增加对未处理命令类型的日志和错误处理
                LOG_ERROR("Unsupported command type encountered for motor '{}'.", motorId);
                success = false; // 遇到不支持的命令也视为失败
            }
        }, cmd);

        if (!success) {
            LOG_WARN("Command execution failed for motor '{}'. Aborting sequence.", motorId);
            return false;
        }
    }
    LOG_INFO("Command sequence executed successfully for motor '{}'.", motorId);
    return true;
}
```

------

### 3. 重构代码 (Refactor 阶段)

目前的 `executeCommandSequence` 方法中的 `if constexpr` 结构随着命令类型的增加会变得越来越长。这是一个很好的重构点。我们可以考虑使用函数指针、命令模式的更通用实现或者一个查找表来简化它。

**重构建议：使用一个命令处理器映射**

我们可以定义一个映射，将 `std::variant` 的 `index()` 与对应的处理函数关联起来，或者将命令转换为一个更通用的 `ICommandProcessor` 接口。对于 `std::visit` 内部的 `if constexpr`，目前它的可读性尚可，但在命令数量继续增加时，可以考虑将其分解为独立的访问者类。

**本次重构：**

1. **添加 `else` 分支处理未知的 `Command` 类型**：这可以捕获将来可能添加但尚未处理的命令类型，并提供日志警告。你已经在 `Green` 阶段的代码中添加了这一点，这很好。
2. **确保错误处理一致**：当任何一个命令失败时，立即中断并返回 `false`。这也已在 `Green` 阶段实现。

**对于 `BusinessLogic::executeCommandSequence`，我们目前保持 `if constexpr` 的结构。** 在未来添加更多命令（如点动和角度移动）后，如果 `if constexpr` 链变得过于冗长，我们可以再进行一次重构。现在它仍是清晰和可维护的。

------

### 接下来

请按照以下步骤进行：

1. **更新 `core/MovementCommand.h`**：添加 `AbsoluteMove` 结构体并将其加入 `Command` 的 `std::variant`。
2. **更新 `core/IMotor.h`**：添加 `virtual bool absoluteMove(double targetMm) = 0;`。
3. **更新 `tests/mocks/MockMotor.h`**：添加 `MOCK_METHOD(bool, absoluteMove, (double targetMm), (override));`。
4. **更新 `tests/test_businesslogic.cpp`**：添加 `TEST_F(BusinessLogicTest, ExecutesAbsoluteMovement)` 测试用例。
5. **更新 `core/BusinessLogic.cpp`**：在 `executeCommandSequence` 中添加 `else if constexpr (std::is_same_v<T, AbsoluteMove>)` 的处理逻辑。

完成这些修改后，**编译并运行你的测试**。如果一切顺利，所有测试应该都会通过（Green）。

接下来，我们将继续 **目标3：点动移动**。

好的，我们继续 TDD 流程，实现 **目标3：点动移动**。

------



## 目标3：点动移动



点动移动（Jogging/Inching）通常是指以较慢的速度持续移动，直到接收到停止指令或达到某个限制。为了实现正向和负向，我们需要在命令中包含方向信息。

### 1. 明确需求与编写测试用例 (Red 阶段)

#### 需求描述

新增的点动移动功能：

- 电机可以向**正方向**点动。
- 电机可以向**负方向**点动。
- 点动操作需要提供一个速度。

#### 抽象为命令结构

为了支持点动，并区分方向，我们需要一个新的命令结构。这里我们引入 `Jog` 命令，并用一个布尔值 `positiveDirection` 来表示方向。

```C++
// core/MovementCommand.h (新增 Jog 结构体)
#ifndef MOVEMENT_COMMAND_H
#define MOVEMENT_COMMAND_H

#include <variant>
#include <vector>

// ... (SetSpeed, RelativeMove, Wait, GoHome, AbsoluteMove 保持不变) ...

// 定义 Jog 命令的结构体
struct Jog {
    double speed_mm_per_sec; // 点动速度
    bool positiveDirection;  // true 为正向，false 为负向
};

// 使用 std::variant 定义 Command 类型，新增 Jog
using Command = std::variant<SetSpeed, RelativeMove, Wait, GoHome, AbsoluteMove, Jog>; // <-- 新增 Jog

// 定义命令序列
using CommandSequence = std::vector<Command>;

#endif // MOVEMENT_COMMAND_H
```

#### 修改 `IMotor` 接口

`IMotor` 接口需要添加一个支持点动操作的纯虚函数。

```C++
// core/IMotor.h (新增 jog 函数)
#ifndef IMOTOR_H
#define IMOTOR_H

class IMotor {
public:
    virtual ~IMotor() = default;

    virtual bool setSpeed(double mmPerSec) = 0;
    virtual bool relativeMove(double mm) = 0;
    virtual void wait(int ms) = 0;
    virtual bool goHome() = 0;
    virtual bool absoluteMove(double targetMm) = 0;
    virtual bool jog(double speedMmPerSec, bool positiveDirection) = 0; // <-- 新增的纯虚函数
};

#endif // IMOTOR_H
```

#### 修改 `MockMotor`

同步更新 `MockMotor`，添加对 `jog` 的 Mock。

```C++
// tests/mocks/MockMotor.h (新增 jog 的 Mock)
#ifndef MOCK_MOTOR_H
#define MOCK_MOTOR_H

#include <gmock/gmock.h>
#include "IMotor.h"

class MockMotor : public IMotor {
public:
    MOCK_METHOD(bool, setSpeed, (double mmPerSecond), (override));
    MOCK_METHOD(bool, relativeMove, (double mm), (override));
    MOCK_METHOD(void, wait, (int ms), (override));
    MOCK_METHOD(bool, goHome, (), (override));
    MOCK_METHOD(bool, absoluteMove, (double targetMm), (override));
    MOCK_METHOD(bool, jog, (double speedMmPerSec, bool positiveDirection), (override)); // <-- 新增的 Mock
};

#endif // MOCK_MOTOR_H
```

#### 编写测试用例 (`tests/test_businesslogic.cpp`)

我们将添加两个新的测试用例，分别测试正向和负向的点动。在 `BusinessLogic::executeCommandSequence` 未实现对 `Jog` 的处理之前，这些测试用例会失败（Red）。

```C++
// tests/test_businesslogic.cpp (新增 ExecutesPositiveJog 和 ExecutesNegativeJog 测试)

// ... (SetUp/TearDown 等夹具内容保持不变) ...

// 继续在 BusinessLogicTest 夹具中添加新的 TEST_F
TEST_F(BusinessLogicTest, ExecutesPositiveJog) {
    // 预期行为：点动正向 (速度 5.0 mm/s)
    InSequence s;

    // 期望 jog(5.0, true) 被调用一次，并返回 true
    EXPECT_CALL(*mockMotorRawPtr, jog(5.0, true)).WillOnce(Return(true));

    // 准备命令序列，包含 Jog 命令
    CommandSequence commands;
    commands.push_back(Jog{5.0, true}); // 正向点动 5.0 mm/s

    // 调用被测试的业务逻辑方法
    bool result = businessLogic->executeCommandSequence("main_motor", commands);

    // 断言整个命令序列执行成功
    ASSERT_TRUE(result);
    LOG_INFO("Test 'ExecutesPositiveJog' finished successfully.");
}

TEST_F(BusinessLogicTest, ExecutesNegativeJog) {
    // 预期行为：点动负向 (速度 3.0 mm/s)
    InSequence s;

    // 期望 jog(3.0, false) 被调用一次，并返回 true
    EXPECT_CALL(*mockMotorRawPtr, jog(3.0, false)).WillOnce(Return(true));

    // 准备命令序列，包含 Jog 命令
    CommandSequence commands;
    commands.push_back(Jog{3.0, false}); // 负向点动 3.0 mm/s

    // 调用被测试的业务逻辑方法
    bool result = businessLogic->executeCommandSequence("main_motor", commands);

    // 断言整个命令序列执行成功
    ASSERT_TRUE(result);
    LOG_INFO("Test 'ExecutesNegativeJog' finished successfully.");
}

// ... (其他测试用例保持不变) ...
```

------

### 2. 编写最小化代码使其通过测试 (Green 阶段)

现在，我们将修改 `BusinessLogic::executeCommandSequence` 方法，以处理新的 `Jog` 命令，并使其通过 `ExecutesPositiveJog` 和 `ExecutesNegativeJog` 测试。

#### 修改 `core/BusinessLogic.cpp`

```C++
// core/BusinessLogic.cpp (更新 executeCommandSequence 以处理 Jog)

// ... (BusinessLogic 的构造函数、析构函数以及 executeCommandSequence 的开头保持不变) ...

bool BusinessLogic::executeCommandSequence(const std::string& motorId, const CommandSequence& commands) {
    // ... (查找电机、初始化 success 等保持不变) ...

    LOG_INFO("Executing command sequence for motor '{}' ({} commands).", motorId, commands.size());

    for (const auto& cmd : commands) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, SetSpeed>) {
                LOG_DEBUG("Setting speed for motor '{}' to {} mm/s.", motorId, arg.mm_per_sec);
                if (!motor->setSpeed(arg.mm_per_sec)) {
                    LOG_ERROR("Failed to set speed for motor '{}'.", motorId);
                    success = false;
                }
            } else if constexpr (std::is_same_v<T, RelativeMove>) {
                LOG_DEBUG("Moving motor '{}' by {} mm.", motorId, arg.delta_mm);
                if (!motor->relativeMove(arg.delta_mm)) {
                    LOG_ERROR("Failed to move motor '{}'.", motorId);
                    success = false;
                }
            } else if constexpr (std::is_same_v<T, Wait>) {
                LOG_DEBUG("Waiting for {} ms...", arg.milliseconds);
                motor->wait(arg.milliseconds);
            } else if constexpr (std::is_same_v<T, GoHome>) {
                LOG_DEBUG("Homing motor '{}'...", motorId);
                if (!motor->goHome()) {
                    LOG_ERROR("Failed to home motor '{}'.", motorId);
                    success = false;
                }
            } else if constexpr (std::is_same_v<T, AbsoluteMove>) {
                LOG_DEBUG("Moving motor '{}' to absolute position {} mm.", motorId, arg.target_mm);
                if (!motor->absoluteMove(arg.target_mm)) {
                    LOG_ERROR("Failed to move motor '{}' to absolute position {}.", motorId, arg.target_mm);
                    success = false;
                }
            } else if constexpr (std::is_same_v<T, Jog>) { // <-- 新增的 Jog 处理
                const char* direction = arg.positiveDirection ? "positive" : "negative";
                LOG_DEBUG("Jogging motor '{}' in {} direction at {} mm/s.", motorId, direction, arg.speed_mm_per_sec);
                if (!motor->jog(arg.speed_mm_per_sec, arg.positiveDirection)) {
                    LOG_ERROR("Failed to jog motor '{}' in {} direction at {} mm/s.", motorId, direction, arg.speed_mm_per_sec);
                    success = false;
                }
            } else {
                LOG_ERROR("Unsupported command type encountered for motor '{}'.", motorId);
                success = false;
            }
        }, cmd);

        if (!success) {
            LOG_WARN("Command execution failed for motor '{}'. Aborting sequence.", motorId);
            return false;
        }
    }
    LOG_INFO("Command sequence executed successfully for motor '{}'.", motorId);
    return true;
}
```

------

### 3. 重构代码 (Refactor 阶段)

`executeCommandSequence` 中的 `if constexpr` 链正在变得越来越长。虽然对于目前少数几个命令类型来说尚可接受，但随着命令数量的持续增加，它将变得难以维护和扩展。

**本次重构的思考：**

为了未来的可扩展性，可以考虑将每个命令的处理逻辑抽象成更小的、独立的函数或对象。例如，可以创建一个 `CommandProcessor` 接口，并为每个 `Command` 类型创建一个实现类。然后，`std::visit` 内部的逻辑将变成调用一个统一的 `process(motor, command)` 方法。

但是，遵循 TDD 的“最小化代码”原则，在代码功能完全实现并通过测试之后，才进行大规模重构。对于当前 `Jog` 命令的添加，`if constexpr` 仍是最直接的实现方式。我们可以在所有**核心命令类型都实现并通过测试**后，再进行一次整体的重构，以提高代码的清晰度和可维护性。

**目前，我们暂时不进行大规模重构，继续保持 `if constexpr` 结构。**

------

### 接下来

请按照以下步骤进行：

1. **更新 `core/MovementCommand.h`**：添加 `Jog` 结构体并将其加入 `Command` 的 `std::variant`。
2. **更新 `core/IMotor.h`**：添加 `virtual bool jog(double speedMmPerSec, bool positiveDirection) = 0;`。
3. **更新 `tests/mocks/MockMotor.h`**：添加 `MOCK_METHOD(bool, jog, (double speedMmPerSec, bool positiveDirection), (override));`。
4. **更新 `tests/test_businesslogic.cpp`**：添加 `TEST_F(BusinessLogicTest, ExecutesPositiveJog)` 和 `TEST_F(BusinessLogicTest, ExecutesNegativeJog)` 测试用例。
5. **更新 `core/BusinessLogic.cpp`**：在 `executeCommandSequence` 中添加 `else if constexpr (std::is_same_v<T, Jog>)` 的处理逻辑。

完成这些修改后，**编译并运行你的测试**。确保所有测试（包括旧的和新的）都通过。

接下来，我们将继续 **目标4：角度位置移动**。

## 目标4：角度位置移动

引入适配器模式 (`IServoAdapter`)。