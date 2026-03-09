核心原则：

> servoV6 不只是一个项目，而是 **你的 C++训练平台**。

------

# servoV6 开发规范（C++能力强化版）

目标：

```text
1 提升 C++ / STL / 架构能力
2 保证系统长期可维护
3 构建可持续成长的工程习惯
```

原则：

```text
代码 ≠ 只是实现功能
代码 = 训练工程能力
```

| 层             | 是否允许 Qt |
| -------------- | ----------- |
| UI             | ✅           |
| ViewModel      | ⚠️           |
| Application    | ❌           |
| Domain         | ❌           |
| Infrastructure | ❌           |

------

# 一、架构总原则

servoV6 采用分层架构：

```
UI (Qt/QML)

Presentation (ViewModel)

Application (UseCase)

Domain (业务模型)

Infrastructure (通信/驱动)
```

依赖方向：

```
UI
 ↓
Presentation
 ↓
Application
 ↓
Domain
```

禁止反向依赖。

------

# 二、Domain 层开发规范（最重要）

Domain 是 **C++能力训练核心区**。

目标：

```
训练 STL
训练对象建模
训练算法思维
```

## 2.1 禁止规则

### 禁止 new / delete

```
禁止：

new
delete
裸指针所有权
```

必须使用：

```c++
std::unique_ptr
std::shared_ptr
std::vector
std::optional
```

目的：

```
强制理解 RAII
理解资源管理
```

------

### 禁止 Qt 类型

```
禁止：

QString
QList
QObject
QVariant
signals
slots
```

必须使用：

```
std::string
std::vector
std::map
```

目的：

```
保持 Domain 纯 C++
```

------

### 禁止寄存器地址

```
禁止：

3004
3005
0x10
```

Domain 只能表达：

```
AxisMoveCommand
MotorState
Alarm
```

目的：

```
避免协议污染业务
```

------

### 禁止 God Function

```
单函数不超过 40 行
```

原因：

```
强制拆分抽象
```

------

### 禁止 bool 表达复杂状态

禁止：

```
bool moving
bool fault
bool enable
```

必须使用：

```
enum class AxisState
{
    Idle,
    Moving,
    Error
};
```

目的：

```
训练状态建模能力
```

### 可测试性质

Domain禁止：

```
printf
cout
日志
文件
线程
锁
网络
```

原因：

```
Domain必须100%可测试
```

------

# 三、STL 强制使用规则

Domain 中必须主动使用 STL。

## 3.1 容器

优先使用：

```
std::vector
std::map
std::array
std::optional
```

避免：

```
C数组
裸指针数组
```

------

## 3.2 算法

优先使用：

```
std::find_if
std::transform
std::copy_if
std::max_element
```

避免：

```
for + if
```

目标：

```
提升 STL 熟练度
```

------

# 四、Application 层规范

Application 层职责：

```
协调 Domain
实现 UseCase
```

例如：

```
MoveAxisUseCase
ConnectPlcUseCase
JogAxisUseCase
```

规则：

### 不允许

```
寄存器
Modbus
Qt
```

只做：

```
业务流程
```

例如：

```
检查状态
调用 Domain
调用 Gateway
```

------

# 五、ViewModel 层规范

ViewModel 是：

```
Qt 与 C++ 的适配器
```

职责：

```
UI输入
↓
调用 UseCase
↓
更新 UI
```

禁止：

```
业务逻辑
```

ViewModel 只允许：

```
数据转换
```

例如：

```
QString -> std::string
```

------

# 六、Infrastructure 层规范

Infrastructure 负责：

```
Modbus
TCP
驱动
寄存器
```

禁止：

```
业务逻辑
```

Infrastructure 只能做：

```
通信
数据转换
```

------

# 七、测试规范（TDD）

servoV6 的测试重点：

```
Domain
Application
```

必须覆盖：

```
Axis
MotorService
CommandProcessor
UseCase
```

不测试：

```
Qt UI
```

原因：

```
UI 测试成本过高
```

------

# 八、日志规范

日志分三类：

```
System Log
Device Log
Business Log
```

例如：

```
[Axis] move start target=100
[PLC] read register 3004
[Alarm] axis overcurrent
```

目标：

```
快速定位问题
```

------

# 九、代码复杂度控制

每个类职责必须单一。

一个类建议：

```
< 300 行
```

一个函数：

```
< 40 行
```

原因：

```
提高可读性
```

------

# 十、工程成长纪律（最重要）

servoV6 不是普通项目。

它是你的：

```
C++训练场
```

所以必须坚持：

### 每周

```
至少使用 3 个 STL 算法
```

例如：

```
transform
copy_if
max_element
```

------

### 每周

重构 1 个模块：

```
简化结构
提升抽象
```

------

### 每周

写至少：

```
2 个单元测试
```

------

# 十一、最重要的一条规则

servoV6 的核心原则：

```
能用 STL 就不用 for
能用 RAII 就不用 new
能用对象建模就不用 if
```

------

