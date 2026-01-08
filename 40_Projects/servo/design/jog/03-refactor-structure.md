# 保守重构思路

## 当前阶段的总目标

现在**不适合**做这些事：

❌ 不要马上引入复杂策略模式
❌ 不要尝试抽 UI / MessageBox
❌ 不要为了“优雅”拆成十几个类
❌ 不要指望立刻能单元测试

👉 **你的阶段目标只有一个：**

> **让 `checkJog` 里“为什么停”和“怎么停”分开**

只要做到这一点，后面的 Z 轴组合限位就不再恐怖。

## 原来的 checkJog 在干什么？

混在一起做了 5 件事：

1. 读硬件状态
2. 判断业务规则
3. 决定是否停止
4. 执行 stop
5. 弹 UI

👉 这就是「结构膨胀、复制粘贴、害怕加逻辑」的根源。

------

## 重构后的 checkJog 只做 3 件事

```tex
1. 轮询 / 采样
2. 判断是否出现 JogStopReason
3. 一旦出现 → 交给 stopJog
```

------

## 定义 StopReason

首先可以先在 `MotorCtrl` 里定义：

```C++
enum class JogStopReason {
    None,  // 特殊值：表示无事件
    
    // 第一类：同步与使能问题（设备状态异常）
    DualMotorOutOfSync,               // 双电机不同步（比NotSync更常用）
    DualMotorEnableTimeout,           // 电机使能不同步
    
    // 第二类：通用安全限位（正常的安全保护）
    ExceedSoftLimit,              // 超出软件保护限位
    ExceedPhysicalLimit,          // 超出硬件物理限位
    
	// 第三类：障碍物区域协同安全
	XZ_CantPassObstacle_ZAxisTooHigh,         // 无法通过障碍物：Z太高
	XZ_CantPassObstacle_XAxisNotInSafeZone,   // 无法在障碍区上升
    
    // 第四类：系统异常（不可预期的故障）
    SpeedReadFailed,            // 速度传感器故障
};
```

这个领域模型是通过完成了足够充分的业务抽象讨论后决定的。

> **定义 enum ≠ 重构完成**
>  真正的第一步是：
>  **让 checkJog“只返回原因，不直接停止”**

## Step 1️⃣ 引入 Stop 事件承载体（最小）

```C++
struct JogStopEvent {
    JogStopReason reason = JogStopReason::None;
    int motorID = -1;
    
    // 默认构造函数
    JogStopEvent() = default;
    
    // 两个参数的构造函数
    constexpr JogStopEvent(JogStopReason r, int id = -1) noexcept 
        : reason(r), motorID(id) {}
    
    // 检查是否有停止事件
    bool hasEvent() const { return reason != JogStopReason::None; }
    
    // 获取描述信息
    QString toString() const;
};
```

👉 必须存在这一步

👉 它是你从“流程代码”迈向“事件模型”的门槛

------

## Step 2️⃣ 给 checkJog 一个“新的内心目标”

### 以前 checkJog 的目标是：

> 在一堆 if 里，尽快 stop + break

### 现在 checkJog 的目标是：

> **在一次循环中，找到第一个 JogStopEvent**

------

## Step 3️⃣ 把“停止判断”拆成**可顺序执行的规则函数**

不是重构，是**平移**。

### 示例：双电机同步

```C++
MotorCtrl::JogStopEvent MotorCtrl::checkDualMotorState()
{
    float v1, v2;
    if (isX1X2NotSync(&v1, &v2)) {
        return JogStopEvent(JogStopReason::DualMotorOutOfSync, M_ALL);
    }
    if (isX1X2NotEnableOnTime()) {
        return JogStopEvent(JogStopReason::DualMotorEnableTimeout, M_ALL);
    }
    
    // 返回默认构造的事件（无事件）
    return JogStopEvent();
}
```

------

### 示例：软件限位

```C++
JogStopEvent MotorCtrl::checkSoftLimit(int motorID)
{
    float curr = getLocation(motorID) * DbCtrl::m_servoMotor_tb.at(motorID).dir;
    float minLimit = DbCtrl::m_servoMotor_tb.at(motorID).minLimit;
    float maxLimit = DbCtrl::m_servoMotor_tb.at(motorID).maxLimit;

    if ((m_jog_dir < 0 && curr <= minLimit + 20) ||
        (m_jog_dir > 0 && curr >= maxLimit - 20)) {
        return JogStopEvent(JogStopReason::ExceedSoftLimit, motorID);
    }
    return JogStopEvent();
}
```

------

### 示例：物理限位 / 速度异常

```C++
JogStopEvent MotorCtrl::checkPhysicalLimit(int motorID)
{
    quint16 speed = 0;
    if (!getCurrentMotorActualSpeed(motorID, speed)) {
        return JogStopEvent(JogStopReason::SpeedReadFailed, motorID);
    }
    
    if (speed < 1) {
        return JogStopEvent(JogStopReason::ExceedPhysicalLimit, motorID);
    }
    
    return JogStopEvent();
}
```

------

## Step 4️⃣ 组合限位终于“降级”为普通规则（重点）

### X1X2 运动时

```C++
JogStopEvent MotorCtrl::checkXZObstacleForXAxis()
{
    // 检查是否启用XZ障碍物逻辑
    if (!isXZObstacleEnabled()) {
        return JogStopEvent();
    }
    
    // 判断Z轴是否过高
    if (!isZAxisTooHigh()) {
        return JogStopEvent();
    }
    
    // 检查X轴是否到达障碍物限制
    if (xAxisReachObstacleLimit()) {
        return JogStopEvent(JogStopReason::XZ_CantPassObstacle_ZAxisTooHigh, M_ALL);
    }
    
    return JogStopEvent();
}
```

------

### Z 轴运动时（你之前最痛苦的）

```C++
JogStopEvent MotorCtrl::checkXZObstacleForZAxis()
{
    // 检查是否启用XZ障碍物逻辑
    if (!isXZObstacleEnabled()) {
        return JogStopEvent();
    }
    
    // 检查X轴是否在安全区域
    if (isX1X2InSafeZone()) {
        return JogStopEvent();
    }
    
    return JogStopEvent(JogStopReason::XZ_CantPassObstacle_XAxisNotInSafeZone, M_Z1);
}
```

👉 **没有复制一坨 if**
👉 **没有再引入新的全局变量**
👉 **只是在返回“为什么不能继续”**

------

## Step 5️⃣ checkJog 主循环，变成“规则流水线”

```C++
for (int motorID = 0; motorID < MAX_MOTOR_COUNT; ++motorID) {
    // 检查是否有电机需要监控
    if (!m_needCheck.at(motorID)) {
        continue;
    }

    anyMotorCheckedInThisCycle = true;

    // ===================== 规则流水线 =====================
    
    // 1. 双电机状态检查
    if (m_funSl_btgrp->checkedId() == FrontBack && 
        m_frontBackModel_btgrp->checkedId() == BothMotors) {
        if (auto ev = checkDualMotorState(); ev.hasEvent()) {
            return stopJog(ev);
        }
    } else {
        // 单电机模式：刷新当前位置
        QLOG_INFO() << "刷新电机当前位置...";
        refreshRealLoc(motorID);
    }

    // 2. 软件限位检查
    if (auto ev = checkSoftLimit(motorID); ev.hasEvent()) {
        return stopJog(ev);
    }

    // 3. 物理限位/速度异常检查
    if (auto ev = checkPhysicalLimit(motorID); ev.hasEvent()) {
        return stopJog(ev);
    }

    // 4. X轴障碍物协同检查（只在需要时检查）
    if (is_z1_out_saftey_area && isXAxis(motorID)) {
        if (auto ev = checkXZObstacleForXAxis(motorID); ev.hasEvent()) {
            return stopJog(ev);
        }
    }

    // 5. Z轴障碍物协同检查
    if (isZAxis(motorID)) {
        if (auto ev = checkXZObstacleForZAxis(); ev.hasEvent()) {
            return stopJog(ev);
        }
    }
}
```

------

## Step 6️⃣ stopJog 成为唯一出口（真正的结构性收益）

```C++
void MotorCtrl::stopJog(const JogStopEvent& ev)
{
    m_allDone = true;
    actionJogStop();

    // 记录日志
    QLOG_WARN() << jogStopReasonToString(ev);

    // 弹出对话框
    SMessageBox::sQdialogBoxOk(
        this,
        QMessageBox::Critical,
        jogStopReasonToHumanReadable(ev)
    );

    // 原循环结束后的动作
    QLOG_INFO()<<QString("行车点动运行结束.");
    emit em_jogDone(true);
}
```