# 系统架构设计说明

## 1. 项目背景

本项目为真实生产环境使用的工业控制系统。

功能包括：

- PLC 通信（Modbus TCP）
- 伺服控制
- 位置读取与运动控制
- 状态监控与报警处理
- QtQuick 人机界面

目标：

- 长期稳定运行
- 可扩展多设备
- 易于维护与测试
- 分层清晰，避免协议污染业务逻辑

---

## 2. 总体架构分层

系统采用 Clean Architecture 分层模型。

### 2.1 分层结构

UI 层  
→ Application 层  
→ Domain 层  
→ Infrastructure 层  

### 2.2 依赖方向

- UI 依赖 Application
- Application 依赖 Domain
- Infrastructure 实现 Domain 定义的接口
- Domain 不依赖任何具体实现

严禁反向依赖。

---

## 3. 各层职责说明

### 3.1 UI 层（QtQuick）

职责：

- 展示界面
- 用户输入
- 状态绑定
- 不包含业务逻辑
- 不包含通信逻辑

UI 通过 ViewModel / Application Service 与下层交互。

---

### 3.2 Application 层

职责：

- 协调领域对象
- 处理用例（Use Case）
- 管理操作流程
- 连接 UI 与 Domain

例如：

- ConnectPlcUseCase
- MoveServoUseCase
- ReadPositionUseCase

Application 不处理寄存器细节。

---

### 3.3 Domain 层

职责：

- 表达业务规则
- 定义领域对象
- 定义接口抽象
- 管理状态机

示例对象：

- PlcDevice
- ServoMotor
- Axis
- MoveCommand
- AlarmState

Domain 层禁止：

- Qt 类型
- Modbus 类型
- 网络 API

---

### 3.4 Infrastructure 层

职责：

- 实现通信接口
- 实现 Modbus TCP
- 实现具体数据读写
- 处理网络异常
- 数据转换与寄存器映射

示例：

- ModbusTcpTransport
- PlcRegisterMapper

Infrastructure 不能包含业务规则。

---

## 4. 通信模型设计

### 4.1 通信抽象

Domain 定义接口：

- ITransport
- IPlcGateway

Infrastructure 实现：

- ModbusTcpTransport
- MockTransport（测试用）

---

### 4.2 寄存器映射策略

所有寄存器地址统一集中管理。

禁止在多个模块硬编码寄存器地址。

建议建立：

- RegisterMap 结构
- 地址常量统一维护

---

## 5. 线程模型

建议模型：

- 通信线程独立
- UI 线程只做展示
- Application 层协调跨线程数据

禁止：

- UI 线程直接访问网络
- Domain 中处理线程切换

---

## 6. 状态管理模型

所有设备状态必须显式建模：

- 未连接
- 已连接
- 运动中
- 报警
- 故障

禁止使用布尔变量拼凑状态。

建议使用状态枚举或状态机模式。

---

## 7. 异常与错误处理

- Infrastructure 层负责捕获通信异常
- 转换为统一错误模型
- 由 Application 层决定如何处理
- UI 只展示结果

禁止在 UI 层捕获底层异常。

---

## 8. 日志策略

- 通信日志
- 状态变更日志
- 错误日志

日志必须可定位到：

- 设备
- 操作
- 时间点

---

## 9. 架构约束规则

1. 不允许跨层直接访问
2. 不允许寄存器污染领域层
3. 不允许 UI 包含业务规则
4. 不允许 Infrastructure 控制业务流程
5. 所有重大架构决策必须记录到 decision_log.md

---

## 10. 当前架构风险点

（此处持续更新）

---

## 11. 演进策略

未来扩展方向：

- 多 PLC 支持
- 多协议支持
- 插件式设备模型
- 可配置流程控制