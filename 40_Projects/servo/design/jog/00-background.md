# checkJog 重构背景

## 原始业务目标

checkJog 的初始设计目标相对明确：

- 在点动过程中，周期性轮询电机状态
- 一旦检测到异常或风险条件，立即停止点动
- 以“安全优先”为原则，避免设备或人身风险

在早期阶段，点动逻辑主要服务于**单电机、单一限位条件**，因此采用了**“循环 + 条件判断 + 立即停止”**的直接实现方式。

## 业务复杂化来源

随着设备形态和使用场景的演进，点动控制逐步从“简单动作控制”演变为一个**包含多轴联动、安全约束、硬件异常处理的综合监控流程**。

### 1. 双电机安全性

在 X1 / X2 双电机模式下，点动不再是“单一电机行为”，而是一个**同步系统**：

- 需要确认双电机是否在规定时间内完成运动使能
- 需要持续检测双电机位置是否同步，避免机械结构受力异常

这类问题的特点是：

- **不是“某一个电机”的问题**
- 而是“系统状态是否仍然可信”

因此，点动停止的触发条件从“电机自身状态”扩展到了**电机之间的关系状态**。

### 2. 其他电机的通用安全检查

对于单电机及 X1X2 场景，点动过程中还需要持续监控：

- 是否接近或到达软限位（min / max limit）
- 是否触发物理限位（通过转速 < 1 间接判断）

这一阶段引入的复杂性在于：

- 停止条件不再只有“一个 if”
- 而是多个**并列、独立但同等重要的安全规则**

每新增一条规则，都会直接叠加到原有流程中。

### 3. 特殊的组合限位需求（X1X2 ↔ Z）

当 X1X2 与 Z 轴同时配置了组合限位后，
 点动逻辑首次出现了**“跨轴依赖”**的业务特性。

#### X1X2 运动时：

- 需要先判断 Z 轴是否已经处于危险高度
  - 若 Z 已超出组合限位：
    - X1X2 点动过程中，需实时检测是否进入组合安全区
  - 若 Z 未超出组合限位：
    - X1X2 可按常规流程点动

#### Z 轴运动时：

- 需要判断 X1X2 是否位于组合限位范围内
  - 若 X1X2 在安全区内：
    - 需持续监控 Z 是否即将超限
  - 若 X1X2 不在安全区内：
    - Z 点动不受该组合规则约束

这一阶段的关键变化是：

> 点动是否安全，不再只取决于“当前运动轴”，
>  而取决于**另一轴的实时状态**。

这使得点动停止条件开始具有**上下文依赖性**。

### 4. 其他逻辑

在实际实现过程中，checkJog 还逐步承载了：

- UI 提示（弹窗）
- 日志输出
- 停止动作的直接触发

这些行为与“安全判断”紧密耦合在一起，使得：

- 判断逻辑难以复用
- 停止原因难以统一描述
- 后续维护需要同时理解业务规则与界面行为

## 当前阶段的关键问题认知（重构动机）

随着以上规则不断叠加，checkJog 函数逐渐呈现出以下特征：

- 停止条件以 if / break 的形式**分散在流程中**
- “为什么停止”只能通过回溯代码路径才能推断
- 新增或修改安全规则时，难以确认是否覆盖所有分支
- 控制逻辑、业务规则、UI 行为高度耦合

这表明：

> 当前实现方式已经无法清晰表达
>  “点动停止”这一核心业务概念本身。

因此，有必要对 checkJog 进行一次**以业务抽象为核心的重构**，

将“点动停止条件”从流程控制中显式建模出来，为后续功能扩展和安全维护提供稳定结构。

## checkJog 代码附录

142行的恐怖代码：

```C++
void MotorCtrl::checkJog()
{
    QLOG_INFO()<<QString("行车点动运动状态检查线程启动");
    McuCtrl::running();

    // 重置点动操作完成标志。此标志设为 false，表明当前有点动操作正在进行。
    m_jog_done = false;

    bool is_z1_out_saftey_area = false;
    float z1_position = 0;
    int z1_motorID = GroupManager::getModbusID(M_Z1);

    // 双电机的预警区域
    if (m_funSl_btgrp->checkedId()==FrontBack && m_frontBackModel_btgrp->checkedId() == BothMotors) {
        int x1_motorID = GroupManager::getModbusID(M_X1);
        // 如果双电机和Z轴设置了特殊限位，需要读取Z轴位置信息
        if (DbCtrl::m_servoMotor_tb.at(x1_motorID).specialLimit != 0 && DbCtrl::m_servoMotor_tb.at(z1_motorID).specialLimit != 0) {
            // 获取 Z 电机位置
            QLOG_INFO() << "检测电机Z的位置是否在预警限位内";
            z1_position = getLocation(z1_motorID) * DbCtrl::m_servoMotor_tb.at(z1_motorID).dir;

            // 判断是否超出最大特殊预警限位 - Z轴处于危险区域内
            is_z1_out_saftey_area = (z1_position >= DbCtrl::m_servoMotor_tb.at(z1_motorID).specialStop);
            if (is_z1_out_saftey_area) {
                QLOG_INFO() << "Z1轴已经在危险区域内，后续点动检测需要预警X1X2移动流程";
            } else {
                QLOG_INFO() << "Z1轴低于危险区域内，不需要预警X1X2";
            }
        }
    }

    // 主监控循环：只要 'm_allDone' 标志为 false，循环就会继续。
    // 'm_allDone' 在检测到需要停止的条件时（如限位到达、同步问题或外部停止）会被设置为 true。
    do{
        QThread::msleep(100);

        // 标志变量：用于追踪当前循环周期内，是否有电机被配置并实际进行了检查。
        bool anyMotorCheckedInThisCycle = false;

        for(int motorID = 0; motorID < MAX_MOTOR_COUNT; ++motorID){
            // 提前退出机制：如果在之前的检查中或由于外部信号，'m_allDone' 已被设置为 true，
            // 表明整个点动操作需要停止，则直接跳出当前电机遍历循环
            if(m_allDone){
                break;
            }

            // 检查当前电机是否被标记为需要在本次点动操作中进行监控。
            if(m_needCheck.at(motorID)){
                anyMotorCheckedInThisCycle = true;
                if(m_funSl_btgrp->checkedId()==FrontBack && m_frontBackModel_btgrp->checkedId() == BothMotors){
                    float val1, val2;
                    if(isX1X2NotSync(&val1, &val2) || isX1X2NotEnableOnTime()){
                        m_allDone = true;
                        actionJogStop();
                        SMessageBox::sQdialogBoxOk(this, QMessageBox::Critical, "双电机距离误差过大，无法控制移动！\n请刷新界面后确认误差是否在10mm以内！");
                        break;
                    }
                } else {
                    QLOG_INFO() << "刷新电机当前位置...";
                    refreshRealLoc(motorID);
                }
                QLOG_INFO() << QString("判断点动的电机[%1]是否超出限位").arg(DbCtrl::s_motorNameList.at(motorID));
                float curr = getLocation(motorID)*DbCtrl::m_servoMotor_tb.at(motorID).dir;
                if(((m_jog_dir<0) && curr<=(DbCtrl::m_servoMotor_tb.at(motorID).minLimit+20)) ||
                   ((m_jog_dir>0) && curr>=(DbCtrl::m_servoMotor_tb.at(motorID).maxLimit-20))){
                    m_allDone = true;
                    QLOG_INFO() << "超出限位执行点动停止...";
                    actionJogStop();
                    QLOG_WARN()<<QString("电机%1到达限位[%2,%3]，停止运行。 当前位置%4").arg(DbCtrl::s_motorNameList.at(motorID))
                                 .arg(DbCtrl::m_servoMotor_tb.at(motorID).minLimit).arg(DbCtrl::m_servoMotor_tb.at(motorID).maxLimit).arg(curr);
                    SMessageBox::sQdialogBoxOk(this, QMessageBox::Critical, QString("电机%1到达限位[%2,%3]，停止运行。 \n当前位置%4").arg(DbCtrl::s_motorNameList.at(motorID))
                                               .arg(DbCtrl::m_servoMotor_tb.at(motorID).minLimit).arg(DbCtrl::m_servoMotor_tb.at(motorID).maxLimit).arg(curr));
                    break;
                }

                if (is_z1_out_saftey_area) { // Z轴位于警戒区域内
                    if(((m_jog_dir<0) && curr<=(DbCtrl::m_servoMotor_tb.at(motorID).specialStop+20)) ||
                       ((m_jog_dir>0) && curr>=(DbCtrl::m_servoMotor_tb.at(motorID).specialLimit-20))) {
                        m_allDone = true;
                        QLOG_INFO() << "X1X2已经到达Z1预警限位...";
                        actionJogStop();
                        QLOG_WARN()<<QString("电机%1的预警限位[%2,%3]，停止运行。 当前位置%4").arg(DbCtrl::s_motorNameList.at(motorID))
                                     .arg(DbCtrl::m_servoMotor_tb.at(motorID).specialLimit).arg(DbCtrl::m_servoMotor_tb.at(motorID).specialStop).arg(curr);
                        SMessageBox::sQdialogBoxOk(this, QMessageBox::Critical, QString("Z轴处于危险高度，停止运行。 \nZ当前位置%1，请下降到%2以下").arg(z1_position)
                                                   .arg(DbCtrl::m_servoMotor_tb.at(z1_motorID).specialLimit));
                        break;
                    }
                }

                // ===================== 新增：判断转速是否小于1 =====================
                quint16 currentSpeed = 0;
                bool readSpeedSuccess = getCurrentMotorActualSpeed(motorID, currentSpeed);
                if (readSpeedSuccess) {
                    QLOG_INFO() << QString("电机[%1]当前转速：%2 rpm").arg(DbCtrl::s_motorNameList.at(motorID)).arg(currentSpeed);
                    // 转速小于1时触发提示
                    if (currentSpeed < 1) {
                        m_allDone = true;
                        actionJogStop(); // 停止点动
                        QString warnMsg = QString("电机%1触发物理限位停止！\n请检查电机限位状态！")
                                             .arg(DbCtrl::s_motorNameList.at(motorID));
                        // 日志警告
                        QLOG_WARN() << warnMsg;
                        // 弹窗提示
                        SMessageBox::sQdialogBoxOk(this, QMessageBox::Critical, warnMsg);
                        break; // 终止循环
                    }
                } else {
                    m_allDone = true;
                    // 转速读取失败的日志（不弹窗，避免干扰原有逻辑）
                    QString warnMsg = QString("电机%1转速读取失败，无法判断是否小于1 rpm, 开始执行点动结束流程...")
                                         .arg(DbCtrl::s_motorNameList.at(motorID)).arg(currentSpeed);
                    QLOG_WARN() << warnMsg;
                    actionJogStop(); // 停止点动
                    // 日志警告
                    QLOG_WARN() << warnMsg;
                    // 弹窗提示
                    SMessageBox::sQdialogBoxOk(this, QMessageBox::Critical, warnMsg);
                    break; // 终止循环
                }
            }
        }

        // 如果本次循环遍历了所有电机，但 'anyMotorCheckedInThisCycle' 仍然为 false，
        // 这表明没有电机被配置为需要监控。这被视为一种不正常的流程或配置错误。
        if(!anyMotorCheckedInThisCycle){
            m_allDone = true;
            QLOG_WARN() << QString("checkJog：这不是个正确的流程");
            int id = getMotorID();
            if (m_needCheck.at(id)) {
                QLOG_WARN()<<QString("checkJog：电机[%1]被标记为需要在本次点动操作中进行监控，但是并未监控").arg((id==M_ALL)?("X1X2"):(DbCtrl::s_motorNameList.at(id)));
            } else {
                QLOG_WARN()<<QString("checkJog：当前[%1]电机并未被标记为监控对象").arg((id==M_ALL)?("X1X2"):(DbCtrl::s_motorNameList.at(id)));
            }
        }
    }while(!m_allDone);

    QLOG_INFO() << "检测结束，开始点动停止...";

    actionJogStop();
    QLOG_INFO()<<QString("行车点动运行结束.");
    emit em_jogDone(true);
}
```

