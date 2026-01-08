# 引入 MotorStateContext 的必要性与技术价值

### 1. 核心问题背景

在原有的逻辑中，每个校验函数（如限位检查、同步检查、干涉预警）都独立调用底层 IO 接口读取电机位置和速度。这导致了三个核心痛点：

- **IO 膨胀与高延迟**：Modbus 或总线通讯是毫秒级的。多次重复读取同一个寄存器会成倍增加点动触发的响应延迟，导致用户体验卡顿。
- **时间戳不一致性（Temporal Inconsistency）**：由于电机在运动，先后两次读取的位置值存在时间差。用 0 毫秒时的 X1 位置和 10 毫秒时的 X2 位置做差值计算，会导致虚假的“同步误差”报警。
- **高耦合度**：业务逻辑逻辑层深度依赖底层驱动接口，导致代码难以进行离线单元测试。

例如`canStartJog()`函数：

```C++
bool MotorCtrl::canStartJog(int motorID, int dir)
{
    QLOG_INFO() << "点动之前检测流程...";
    // 禁止方向按钮
    disableAllbt(dir);
    if (!m_jog_done) {
//        SMessageBox::sQdialogBoxOk(this, QMessageBox::Warning, "请等待上次点动完全停止");
        QLOG_ERROR() << "上次点动尚未停止，用户快速执行点动";
        return false;
    }

    if(m_funSl_btgrp->checkedId()==FrontBack && m_frontBackModel_btgrp->checkedId() == BothMotors){
        if (!isBothX1X2MotorsCanJog(dir)) {
            return false;
        }
    }

    if(!isPositionWithinLimit(motorID, dir)) return false;
    QLOG_INFO() << "点动之前检测流程成功！";

    return true;
}
```

在`isBothX1X2MotorsCanJog()`中获取了X1X2双电机的位置信息后，

需要再从同级的`isPositionWithinLimit()`中再获取一遍，浪费时间

------

### 2. Context 模式的解决方案

**Context 模式（又称 Snapshot 快照模式）** 通过将“数据采集”与“逻辑判断”彻底解耦，解决了上述问题。

#### A. IO 密集型瓶颈的“降维打击”

**解决方式**：在执行逻辑判断前，根据当前的操作模式（单轴或双轴），一次性预测所有需要的电机 ID，执行批量读取并存入 `MotorStateContext` 结构体。

- **效果**：将原本随校验项增加而线性增长的 IO 耗时（$O(N)$），压缩为恒定的单次批量耗时（$O(1)$）。

#### B. 保证逻辑判断的“空间原子性”

**解决方式**：Context 记录了某一时刻设备状态的“全景快照”。

- **效果**：后续所有的逻辑校验（速度一致性、位置同步性、Z 轴碰撞预测）都基于同一时间断面的数据。这消除了因读取先后顺序导致的数据偏差，使安全校验更加精确严谨。

#### C. 实现“局部与全局”逻辑的解耦

**解决方式**：

- **单轴逻辑**（如 `isPositionWithinLimit`）只需读取 `ctx.getData(motorID)`。
- **全局逻辑**（如 `isBothX1X2MotorsCanJog`）则读取 `ctx.getData(X1)` 和 `ctx.getData(Z1)`。
- **效果**：主控函数 `canStartJog` 像一个调度员，根据业务场景动态填充 Context。子函数不再关心数据从哪来（IO 还是缓存），只关心如何算。这使得代码结构从“面条式耦合”转变为“层级化组合”。

------

### 3. 重构前后的定量对比

| **评价维度**   | **重构前（独立 IO 读取）**            | **重构后（Context 快照）**              |
| -------------- | ------------------------------------- | --------------------------------------- |
| **平均延迟**   | $N \times \text{Modbus Latency}$ (高) | $1 \times \text{Batch Latency}$ (低)    |
| **数据一致性** | 差（存在时间差导致的计算偏差）        | **极高（同一时刻快照）**                |
| **代码复用性** | 差（函数签名绑定了驱动接口）          | **强（逻辑函数仅依赖纯数据结构）**      |
| **可维护性**   | 难（增加一个判断需多写几次读取）      | 易（只需在入口处扩展 Context 填充逻辑） |

------

### 4. 结论总结

引入 `Context` 模式不仅是代码整洁度的优化，更是对**实时性控制系统**鲁棒性的提升。

它将昂贵的 IO 操作转化为廉价的内存操作，确保了安全校验逻辑的快速响应与数据准确，

为后续增加更复杂的轴间联动干涉逻辑（如多轴补偿、避障算法）提供了坚实的底层架构支持。