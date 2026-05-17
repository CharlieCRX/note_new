# FakePLC 龙门控制缺陷分析文档

## 一、背景

当前 `FakePLC::tickGantry()` 的实现：

```cpp
void tickGantry(int ms) {
    // 电机使能延迟状态机
    if (m_gantryPowerCmdPending) {
        m_gantryPowerTimer += ms;
        if (m_gantryPowerTimer >= GANTRY_POWER_DELAY_MS) {
            m_gantryFeedback.enable = m_gantryPowerTarget;
            m_gantryPowerCmdPending = false;
            m_gantryPowerTimer = 0;
        }
    }

    // 耦合/解耦延迟状态机
    if (m_gantryCouplingCmdPending) {
        m_gantryCouplingTimer += ms;
        if (m_gantryCouplingTimer >= GANTRY_COUPLING_DELAY_MS) {
            m_gantryFeedback.isCoupled = m_gantryCouplingTarget;
            m_gantryFeedback.errorCode = 0;
            m_gantryCouplingCmdPending = false;
            m_gantryCouplingTimer = 0;
        }
    }
}
```

当前实现本质上只是：

```text
命令 -> 延迟 -> 直接修改反馈
```

这与真实 PLC 的行为存在较大差异。

真实 PLC 中：

```text
控制命令 ≠ 物理结果
```

PLC 内部会：

- 检查轴状态
- 检查速度
- 检查位置差
- 检查报警
- 检查联动条件
- 根据物理状态动态刷新反馈寄存器

而当前 FakePLC 缺少了“真实 PLC 的内部逻辑层”。

------

# 二、当前 FakePLC 的核心问题

## 1. FakePLC 当前是“命令驱动反馈”

当前模型：

```text
收到命令
  ↓
延迟
  ↓
直接改 feedback
```

但真实 PLC：

```text
收到命令
  ↓
进入 PLC 内部状态机
  ↓
持续检查物理状态
  ↓
满足条件才更新反馈寄存器
```

也就是说：

当前 FakePLC：

```text
feedback = command 的镜像
```

真实 PLC：

```text
feedback = 真实物理状态
```

这是当前最大的结构问题。

------

# 三、当前 FakePLC 缺失的关键行为

# 1️⃣ 缺失“联动条件检查”

当前：

```cpp
m_gantryFeedback.isCoupled = m_gantryCouplingTarget;
```

这意味着：

```text
只要下发联动命令
一定联动成功
```

但真实 PLC 中并不是这样。

真实条件：

```text
1. X1 已使能
2. X2 已使能
3. X1 当前速度 = 0
4. X2 当前速度 = 0
5. X1/X2 位置差 < 0.1
6. 无报警
```

只有全部满足：

```text
轴X联动状态 = ON
```

否则：

```text
联动失败
ErrorCode != 0
联动状态仍为 OFF
```

------

## 当前问题导致的后果

当前测试中会出现：

```text
即使 X1/X2 正在运动
FakePLC 仍然会联动成功
```

或者：

```text
即使两轴位置差已经严重超差
仍然联动成功
```

这会导致：

```text
Application / Domain 层的测试全部失真
```

因为真实 PLC 根本不会允许这种情况。

------

# 2️⃣ 缺失“基于轴真实状态的反馈生成”

当前：

```cpp
m_gantryFeedback.enable = m_gantryPowerTarget;
```

但真实 PLC：

```text
轴X状态显示
是根据 X1/X2 当前真实状态实时计算的
```

真实逻辑：

```text
状态0:
    任意轴未使能

状态1:
    双轴使能 + 双轴静止

状态2:
    任意轴运动

状态3:
    报警/限位/超差
```

而当前 FakePLC：

```text
enable 只是一个 bool
没有真实状态机
```

这会导致：

```text
FakePLC 无法表达：
- 正在运动
- 报警
- 超差
- 限位
- 联动失败
```

因此：

```text
FakePLC 当前不是“PLC仿真”
而只是“命令回显器”
```

------

# 3️⃣ 缺失“联动状态自动掉线”机制

真实 PLC：

```text
联动建立后
PLC 会持续检查联动条件
```

例如：

```text
联动过程中：
X1/X2 位置差超过阈值
```

真实 PLC 会：

```text
1. 触发超差报警
2. 关闭联动
3. 停止运动
4. 状态进入报警
```

但当前 FakePLC：

```text
一旦 isCoupled=true
后续永远不会自动解除
```

这是严重缺失。

因为：

```text
龙门控制最核心的就是“持续同步监测”
```

而不是：

```text
联动成功一次就结束
```

------

# 4️⃣ 缺失“超差监测”

当前 FakePLC：

```text
没有实时计算：
ABS(X1.position - X2.position)
```

真实 PLC 中：

```text
这是龙门最核心的安全逻辑
```

当前缺失：

```text
1. 超差阈值寄存器
2. 超差检测
3. 超差报警
4. 超差停机
5. 超差自动解耦
```

而你的需求中已经定义：

```text
轴X报警代码 = 3 → 超差报警
```

但 FakePLC 完全没有实现。

------

# 5️⃣ 缺失“命令拒绝”行为

真实 PLC：

```text
联动条件不满足
不会执行联动命令
```

例如：

```text
X1 未使能
```

真实 PLC：

```text
ErrorCode = X1NotEnabled
联动状态仍然 OFF
```

但当前 FakePLC：

```text
100ms 后直接联动成功
```

因此当前无法测试：

- GantryCouplingController
- GantryGroup
- Application层错误恢复
- UI错误提示
- 超时逻辑
- 重试逻辑

这些全部失真。

------

# 6️⃣ 缺失“PLC扫描周期行为”

真实 PLC：

```text
每个扫描周期：

读取输入
→ 执行逻辑
→ 更新内部状态
→ 刷新输出寄存器
```

但当前 FakePLC：

```text
只是在处理 pending timer
```

缺失：

```text
周期性状态刷新
```

真实 PLC 中：

```text
即使没有新命令
状态寄存器也会不断变化
```

例如：

```text
X1运动起来
→ 轴X状态显示自动变为2

X1停止
→ 自动回到1
```

当前 FakePLC 完全没有这种动态行为。

------

# 7️⃣ 缺失“轴状态聚合”

真实 PLC 中：

```text
轴X状态显示
是 X1/X2 状态聚合后的结果
```

但当前 FakePLC：

```text
龙门状态和轴状态完全割裂
```

例如：

```text
X1 已报警
```

真实 PLC：

```text
轴X状态显示 = 3
```

但当前：

```text
龙门 feedback 不会感知
```

这是当前架构上的一个严重缺口。

------

# 8️⃣ 缺失“联动模式约束”

你的 PLC 需求中已经定义：

```text
绝对定位触发
仅在联动模式下可用
```

当前 FakePLC 并没有：

```text
if (!isCoupled)
    reject absolute move
```

因此：

```text
FakePLC 当前无法模拟：
“联动模式下才能定位”
```

这会导致业务逻辑测试错误。

------

# 9️⃣ 缺失“联动与分动”的运动约束

真实 PLC：

```text
联动 OFF:
    X1/X2 可独立点动

联动 ON:
    X1/X2 不允许独立运动
    必须由龙门统一控制
```

当前 FakePLC：

```text
没有这种运动互斥逻辑
```

因此当前无法测试：

```text
联动锁定
联动保护
联动控制权限
```

------

# 🔟 缺失“联动建立中的中间态”

当前：

```text
请求联动
→ 延迟
→ 直接成功
```

真实 PLC：

```text
收到联动请求
→ 等待双轴停止
→ 检查位置差
→ 同步内部同步器
→ 建立电子齿轮
→ 最终联动成功
```

也就是说：

真实 PLC：

```text
CouplingRequested
并不一定会成功
```

当前 FakePLC：

```text
一定成功
```

这会导致：

```text
GantryCouplingState 的价值被削弱
```

因为 FakePLC 永远不会制造真正的异步失败。

------

# 四、当前 FakePLC 与真实 PLC 的本质差异

当前 FakePLC：

```text
命令驱动模型
```

真实 PLC：

```text
物理状态驱动模型
```

真正的 PLC 核心是：

```text
状态 = 物理现实
```

而不是：

```text
状态 = 上位机意图
```

当前 FakePLC 最大的问题：

```text
它相信命令
而不相信物理状态
```

真实 PLC 刚好相反。

------

# 五、当前 FakePLC 对上层测试的影响

当前会导致：

## 1. Gantry 状态机测试失真

因为：

```text
FakePLC 永远成功
```

所以：

```text
无法测试失败恢复
```

------

## 2. UI 测试失真

因为：

```text
不会出现真实错误码
```

UI 无法测试：

- 超差提示
- 联动失败
- 未使能提示
- 正在运动提示

------

## 3. Application 层测试失真

因为：

```text
永远不会 reject
```

无法测试：

- retry
- timeout
- rollback
- fail-safe
- emergency recovery

------

# 六、FakePLC 应该具备的真实结构

FakePLC 不应该是：

```text
命令 -> feedback
```

而应该是：

```text
Command Register
        ↓
PLC内部逻辑层
        ↓
Physical State
        ↓
Status Register
```

也就是说：

```text
FakePLC 必须拥有：

1. 控制寄存器
2. PLC逻辑层
3. 物理状态
4. 状态寄存器
```

当前缺失的是：

```text
PLC逻辑层
```

------

# 七、建议的 FakePLC 龙门重构方向

建议新增：

## 1. GantryPhysicalState

```cpp
struct GantryPhysicalState {
    bool enable;
    bool coupled;

    bool x1Moving;
    bool x2Moving;

    bool x1Enabled;
    bool x2Enabled;

    bool hasAlarm;

    double positionDelta;

    int errorCode;
};
```

------

## 2. PLC内部逻辑层

例如：

```cpp
void updateGantryState()
```

专门负责：

```text
- 读取X1/X2真实状态
- 计算位置差
- 计算联动条件
- 计算报警
- 计算状态寄存器
```

而不是：

```text
直接修改 feedback
```

------

## 3. 联动建立逻辑

```text
收到联动命令
↓
检查双轴静止
↓
检查双轴使能
↓
检查位置差
↓
满足条件才 coupled=true
否则写 errorCode
```

------

## 4. 联动持续监测

每个 tick：

```text
if coupled:
    检查超差
    检查报警
    检查轴掉电
```

发现异常：

```text
自动解除联动
进入报警状态
```

------

# 八、最终结论

当前 FakePLC 的龙门逻辑：

```text
更像“异步回显器”
而不是“真实PLC仿真器”
```

它缺失了：

```text
1. 联动条件检查
2. 真实状态聚合
3. 超差监测
4. 报警逻辑
5. 联动保护
6. 状态自动刷新
7. 持续同步监测
8. 运动约束
9. PLC扫描周期逻辑
10. 命令拒绝机制
```

而这些：

```text
恰恰才是龙门控制最核心的部分
```

当前 FakePLC 的问题本质：

```text
它在模拟“命令”
但没有模拟“PLC的大脑”
```

因此建议下一阶段：

```text
将 FakePLC 从：

“命令延迟器”

升级为：

“PLC状态机 + 物理仿真器”
```

这样你的：

- Domain
- Application
- GantryStateMachine
- UI
- ErrorRecovery
- Timeout机制

才能真正被工业级验证。