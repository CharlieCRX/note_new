# 工业控制 Clean Architecture 架构模板

> 基于 QtQuick + PLC + ModbusTCP 的工业控制系统架构模板  
> 目标：构建一个可复用、可演进、可测试的工业控制软件基础结构。

---

## 🎯 项目愿景

本项目不仅仅是一个应用程序。

它是一个持续演进的架构模板，用于构建：

- PLC 通信系统
- 伺服与运动控制系统
- QtQuick HMI 控制界面
- 可长期维护的工业软件
- 可测试、可扩展的控制系统

长期目标是形成：

- 一套稳定的工业控制分层架构
- 一套可复用的通信抽象模型
- 一套 TDD 驱动的工程流程
- 一个可开源的架构模板仓库

---

## 🧱 核心设计原则

1. 严格分层（Clean Architecture）
2. 领域模型优先
3. 协议与业务解耦
4. TDD 驱动开发
5. 架构决策可追溯
6. AI 辅助思考，但不替代架构判断

---

## 🏗 系统架构概览

系统采用分层结构：

UI 层  
→ Application 层  
→ Domain 层  
→ Infrastructure 层  

依赖方向：

UI → Application → Domain → Infrastructure  

约束规则：

- Domain 层不依赖 Qt
- Domain 层不依赖 Modbus
- Domain 层不依赖 TCP
- 协议实现细节只能存在于 Infrastructure 层

---

## 🧠 领域模型思想

本项目将工业控制世界显式建模。

示例领域对象：

- PlcDevice（PLC设备）
- ServoMotor（伺服电机）
- AxisPosition（轴位置）
- MoveCommand（运动命令）
- AlarmState（报警状态）
- ConnectionState（连接状态）

领域层表达“行为逻辑”，而不是“寄存器地址”。

---

## 🔌 通信抽象策略

通信通过接口抽象，例如：

- ITransport
- ModbusTcpTransport
- MockTransport（用于测试）

寄存器映射策略仅存在于 Infrastructure 层。

禁止在领域逻辑中出现：

- 原始寄存器地址
- 直接 Modbus 调用
- 原始 uint16 拼接逻辑

---

## 🧪 TDD 开发流程

功能开发遵循以下步骤：

1. 明确功能目标
2. 设计测试顺序
3. 编写最小失败测试
4. 实现最小逻辑
5. 重构
6. 记录架构决策

所有测试规划记录在：

docs/tdd_plan.md

---

## 📂 项目结构

```bash
project/
├── app/ # QtQuick 前端
├── application/ # 应用层
├── domain/ # 领域层
├── infrastructure/ # 通信与协议实现
├── tests/
├── docs/
```

---

## 📘 文档结构说明

- docs/architecture.md  
  系统分层与架构说明  

- docs/domain_model.md  
  领域模型定义与状态机设计  

- docs/protocol_design.md  
  通信抽象与寄存器映射设计  

- docs/tdd_plan.md  
  测试驱动开发规划  

- docs/decision_log.md  
  架构决策记录  

- docs/ai_context.md  
  AI 协作的项目压缩态上下文  

---

## 🤖 AI 协作原则

AI 在本项目中的角色：

- 架构合理性审查
- 测试顺序建议
- 重构建议
- 设计思路讨论

AI 不承担：

- 长期记忆
- 架构决策权
- 全自动代码生成

所有结构性知识必须记录在 docs/ 目录。

---

## 🚀 长期演进方向

未来计划包括：

- 多协议支持（串口 / CAN / OPC UA 等）
- 设备插件机制
- 可复用状态机框架
- 日志与诊断模块
- 家庭自动化扩展探索

---

## 📈 当前状态

本项目处于架构演进阶段。

当前重点：

- 稳定分层结构
- 明确领域模型
- 建立 TDD 节奏
- 沉淀可复用架构模式

---

## 🛠 构建方式

（待补充）

---

## 🧪 测试方式

（待补充）

---

## 📜 许可证

（待确定）