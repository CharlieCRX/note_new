很好，这一步其实非常关键。
**需求说明书 = 项目的“地图”**。
如果没有需求说明书，项目很容易变成：

```
今天想写什么写什么
越写越乱
越写越偏
```

而你本身是做 **工业控制 + 软件架构** 的，这类项目**必须先定义系统边界**。

我会给你一份 **工程级 PLC Simulator 需求说明书（v1.0）**，复杂度控制在：

```
每天 1~1.5h
3个月可完成
```

同时还能：

```
训练 C++
训练架构
未来可以给 servoV6 使用
```

------

# PLC Simulator

Version 1.0
工业 PLC 模拟器需求说明书

------

# 一、项目目标

开发一个 **PLC 模拟服务器**，用于：

```
模拟 PLC 设备
提供 Modbus TCP 通信接口
支持寄存器读写
模拟设备状态变化
```

用途：

1️⃣ **开发 UI 时无需真实 PLC**

2️⃣ **测试通信协议**

3️⃣ **测试 servoV6 系统**

4️⃣ **未来扩展设备模拟**

最终效果：

```
上位机 → Modbus TCP → PLC Simulator
```

客户端可以：

```
读取寄存器
写入寄存器
观察设备状态变化
```

------

# 二、系统整体架构

系统分为 **5 个模块**：

```
PLC Simulator
│
├── Core            核心寄存器系统
│
├── Protocol        协议解析
│
├── Network         TCP通信
│
├── Simulation      设备行为模拟
│
├── UI              监控界面
```

结构：

```
Client
   │
   │ Modbus TCP
   ▼
Network(TcpServer)
   │
   ▼
Protocol(ModbusParser)
   │
   ▼
Core(RegisterManager)
   │
   ▼
Simulation(DeviceLogic)
```

------

# 三、核心功能需求

系统必须实现 **4 个核心功能**。

------

# 1 寄存器系统

PLC 的核心是 **寄存器存储区**。

支持寄存器类型：

```
Coil        线圈 (bool)
Discrete    输入 (bool)
Holding     保持寄存器 (int16)
Input       输入寄存器 (int16)
```

示例地址：

```
00001  Coil
10001  Discrete
40001  Holding Register
30001  Input Register
```

系统需要支持：

```
读取寄存器
写入寄存器
批量读取
批量写入
```

示例：

```
read(40001) → 123
write(40001, 55)
```

------

# 2 Modbus TCP 通信

支持 **Modbus TCP 协议**。

端口：

```
5020
```

支持功能码：

| 功能码 | 功能                  |
| ------ | --------------------- |
| 01     | Read Coil             |
| 03     | Read Holding Register |
| 05     | Write Single Coil     |
| 06     | Write Single Register |

示例：

客户端发送：

```
Read Holding Register
Addr: 40001
Length: 2
```

服务器返回：

```
value1
value2
```

------

# 3 TCP Server

PLC Simulator 必须提供：

```
TCP Server
```

支持：

```
多客户端连接
并发请求
```

服务器流程：

```
启动服务器
等待连接
接收数据
解析 Modbus
操作寄存器
返回响应
```

------

# 4 设备行为模拟

模拟 PLC 内部逻辑。

例如：

### 温度传感器

寄存器：

```
30001 温度
```

行为：

```
每秒增加 0.1
最大 100
```

------

### 电机状态

寄存器：

```
40001 电机使能
40002 电机速度
40003 电机位置
```

逻辑：

```
if enable:
    position += speed * dt
```

------

# 四、UI 功能需求

Qt UI 用于观察系统。

界面显示：

```
寄存器表
设备状态
日志
连接状态
```

示例界面：

```
+---------------------------+
| PLC Simulator             |
+---------------------------+

Registers

Addr      Value
40001     100
40002     50
40003     10234

Device

Motor
Speed: 50
Position: 10234

Temperature
Temp: 36.5

Logs
Client Connected
Read Register 40001
```

------

# 五、日志系统

系统需要基础日志。

日志级别：

```
INFO
WARN
ERROR
```

示例：

```
[INFO] TCP Server Started
[INFO] Client Connected
[INFO] Read Register 40001
[INFO] Write Register 40002
```

------

# 六、配置文件

系统支持配置：

```
端口
寄存器初始值
设备模拟参数
```

例如：

```json
{
  "port":5020,
  "registers":
  {
    "40001":0,
    "40002":0
  }
}
```

------

# 七、项目目录结构

建议结构：

```
plc_simulator
│
├── core
│   ├── RegisterManager
│   ├── MemoryMap
│
├── protocol
│   ├── ModbusParser
│   ├── ModbusResponse
│
├── network
│   ├── TcpServer
│
├── simulation
│   ├── Device
│   ├── MotorSimulator
│   ├── TemperatureSimulator
│
├── ui
│   ├── MainWindow
│
├── logger
│
└── tests
```

------

# 八、系统运行流程

系统启动流程：

```
main
  │
  ├─ 初始化 RegisterManager
  │
  ├─ 启动 SimulationEngine
  │
  ├─ 启动 TcpServer
  │
  └─ 启动 UI
```

通信流程：

```
Client → TCP Server
        ↓
   ModbusParser
        ↓
 RegisterManager
        ↓
 ResponseBuilder
        ↓
     TCP Send
```

------

# 九、非功能需求

系统需要满足：

稳定性：

```
可持续运行
无内存泄漏
```

性能：

```
支持 10+ 客户端
```

代码质量：

```
模块化
可测试
可扩展
```

------

# 十、项目完成标准

项目完成时必须具备：

可以运行：

```
./plc_simulator
```

支持：

```
Modbus TCP
```

客户端测试：

```
Modbus Poll
QModMaster
自写程序
```

能够：

```
读取寄存器
写入寄存器
观察设备模拟
```

UI 可以：

```
实时查看寄存器
查看日志
```

------

# 十一、未来扩展（非本期）

未来版本：

```
OPC UA
Web UI
脚本模拟
自动测试
```

------

# 十二、项目难度评估

这个项目训练能力：

| 能力     | 训练程度 |
| -------- | -------- |
| STL      | ⭐⭐⭐      |
| 网络编程 | ⭐⭐⭐      |
| 协议解析 | ⭐⭐⭐      |
| 架构设计 | ⭐⭐⭐⭐     |
| Qt       | ⭐⭐       |

难度等级：

```
中等偏上
```

但非常适合你。

因为你已经：

```
做工业控制
会 Modbus
会 Qt
```

------

# 十三、给你一个**非常关键的建议**

你现在这个项目 **不要追求复杂**。

目标只有一个：

```
写干净的 C++ 代码
```

不要做：

```
复杂协议
复杂UI
复杂配置
```

只做：

```
最小可运行系统
```

------

如果你愿意，我可以继续给你一个 **非常重要的东西**：

我可以直接帮你设计 **PLC Simulator 的核心架构图（工业级 C++ 结构）**：

里面会包含：

```
RegisterManager
MemoryMap
Device
SimulationEngine
ModbusParser
TcpServer
```

以及 **每个类的职责和接口设计**。

这一步会直接决定：

```
你的代码会不会写成屎山
```