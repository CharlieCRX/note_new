```
infrastructure/
├── network/                              <-- 新增：网络生命周期管理层
│   ├── ConnectionManager.h               <-- 管理心跳、重连、状态机
│   ├── ConnectionManager.cpp
│   └── IConnectionStateListener.h        <-- 网络状态变更回调（供 UI 或报警系统使用）
│
├── modbus/
│   ├── IModbusClient.h                   <-- 纯粹的读写接口（被剥离了重连职责）
│   ├── LibModbusClient.h
│   ├── FakeModbusClient.h
│   ├── RegisterAddress.h                 <-- 更新：引入 RegisterBlock
│   ├── RegisterCodec.h
│   ├── PlcRegisterMap.h                  <-- 更新：返回 RegisterWrite 块
│   ├── PlcFeedbackSnapshot.h             <-- 新增：全局状态快照（防撕裂）
│   ├── ModbusTcpDriver.h                 <-- 核心调度者
│   └── ModbusTcpDriver.cpp
```

这个目录骨架的设计，是经典工业控制软件中“防腐层（Anti-Corruption Layer）”**和**“关注点分离（Separation of Concerns）”原则的完美体现。

在工业项目中，最怕的就是“业务逻辑和底层硬件协议深度耦合”（比如在业务层直接调 `modbus_write(4001, 100)`）。一旦硬件换了、地址改了或者大小端反了，整个系统就会面临重构灾难。

下面我为你逐个拆解这个骨架中每个文件（模块）的作用，以及“为什么要这样设计”的核心逻辑。

### 1. 通讯抽象层（隔离网络与业务）

#### `IModbusClient.h`

- **作用：** 定义与 PLC 通讯的纯虚接口，仅包含 Modbus 协议中最核心的功能码操作（例如 `readHoldingRegisters` 和 `writeHoldingRegisters`），输入输出全都是原始的 `uint16_t`。
- **为什么这么设计：** **依赖倒置原则（DIP）**。系统的其他部分不应该关心你是用 `libmodbus`、`QtSerialPort` 还是自己写的 socket。它们只依赖这个接口。这也为未来的扩展（比如更换为串口 RTU）留下了后路。

#### `FakeModbusClient.h`

- **作用：** `IModbusClient` 的一个“纯内存”替身实现。内部用一个 `std::unordered_map<uint16_t, uint16_t>` 模拟 PLC 的内存寄存器。
- **为什么这么设计：** **100% 离线测试支持（TDD的核心）**。真实的 TCP/IP 调用非常慢，且会超时报错。有了 Fake，你可以在 CI/CD 甚至在完全断网的飞机上跑完所有的自动化测试，验证寄存器写入逻辑是否正确。

### 2. 物理映射与编解码层（解决 Modbus 的固有缺陷）

#### `RegisterAddress.h`

- **作用：** 系统的“单一事实来源（Single Source of Truth）”，用 `constexpr` 集中管理所有的 PLC 寄存器地址偏移量和基址。
- **为什么这么设计：** **消除魔术数字（Magic Numbers）**。在工业现场，PLC 工程师经常会说：“不好意思，X轴的地址块需要整体往后挪 100 个字”。如果你把地址写死在代码各处，这就意味着几天的排错和加班。集中管理后，你只需要在这个文件里改一行代码。

#### `RegisterCodec.h / .cpp`

- **作用：** 字节编解码器。Modbus 协议非常古老，它只认识 16 位的字（`uint16_t`）。这个模块负责把业务层的高级数据类型（`float`, `double`, `int32_t`）切片、组装成 16 位数组。
- **为什么这么设计：** **隔离大小端（Endianness）问题**。PLC 有大端模式（Big-Endian），x86 电脑是小端模式，甚至还有奇怪的“字节交换（Byte Swap）”模式。这是现场调试最容易崩溃的地方（数值变成乱码）。把这种极其底层的位操作隔离在一个专门的类里，配合单元测试，可以保证这部分逻辑绝对可靠。

### 3. 核心翻译层（防腐层核心）

#### `PlcRegisterMap.h / .cpp`

- **作用：** 协议翻译官。负责将你的领域层命令（如 `MoveAbsoluteCommand{target: 100.0, vel: 50.0}`）翻译成具体的写入指令集（如：往 4002 写位置数组，往 4006 写速度数组，往 4001 写启动位 1）。
- **为什么这么设计：** **单一职责原则（SRP）**。这个模块不碰网络，不碰业务状态，它只做纯粹的数据映射。测试这个模块时，你丢进去一个领域对象，检查吐出来的是不是正确的寄存器地址和值。它成功地把“业务语义”和“物理地址”隔离开了。

### 4. 驱动集成层（承上启下）

#### `ModbusTcpDriver.h / .cpp`

- **作用：** 它是 `ISystemDriver` 的具体实现。
  - **下发 (`send`)：** 拿到 `SystemCommand` -> 调用 `PlcRegisterMap` 获取指令集 -> 调用 `IModbusClient` 执行网络写入。
  - **轮询 (`pollFeedback`)：** 调用 `IModbusClient` 批量读取连续几十个寄存器 -> 用 `RegisterCodec` 解码 -> 组装成 `AxisFeedback` 实体 -> 写入 `SystemContext`。
- **为什么这么设计：** 它是系统数据流的**集线器（Hub）**。它的设计强迫了**批量读写**，避免了“需要知道位置就读一次，需要知道速度又读一次”这种灾难性的网络 I/O 性能瓶颈。它把异步的网络状态，转换成了应用层可以随时访问的内存状态（Context）。

### 5. 测试目录层 (`tests/infrastructure/modbus/`)

- **`test_register_codec.cpp`**：只测试字节拼接和大小端，断言一个 `double 123.45` 能拆成 4 个特定的 16位整数，再完美还原。
- **`test_plc_register_map.cpp`**：只测试翻译逻辑，断言发一个“使能”命令，能生成一条 `address = 4000, value = 1` 的写指令结构体。
- **`test_modbus_tcp_driver.cpp`**：注入 `FakeModbusClient`，测试一整套流程。比如给内存地址 4010 塞几个假的位置数据，调用 `pollFeedback` 后，去查 `SystemContext` 里的坐标是不是更新了。

### 总结：数据是如何在这个架构中流动的？

当用户在界面上点击“X轴移动到 100mm”时：

1. **[UI -> Domain]**: `MoveAbsoluteUseCase` 生成一个纯粹的 C++ 结构体 `MoveAbsoluteCommand(X1, 100.0)`，发给驱动。
2. **[Driver -> Map]**: `ModbusTcpDriver` 将这个结构体丢给 `PlcRegisterMap`。
3. **[Map -> Codec]**: `PlcRegisterMap` 发现里面有个 `double 100.0`，调用 `RegisterCodec` 将其转为 4 个 16位寄存器数据。
4. **[Map -> Address]**: `PlcRegisterMap` 查阅 `RegisterAddress`，发现 X 轴目标位置的首地址是 `4002`，组装成翻译结果返回。
5. **[Driver -> Client]**: `ModbusTcpDriver` 拿到翻译结果，调用 `IModbusClient->writeHoldingRegisters(4002, {数据})`。
6. **[Client -> 物理层]**: TCP Socket 将其打包成标准 Modbus 报文，发给真实的 PLC。

**这就是工业级软件健壮性的来源**：每一层都极其简单、愚蠢，且只负责一件事。出了任何 Bug（位置不对、指令没动、断线崩溃），你都能在一秒钟内定位到具体是上面 5 个层级中的哪一层出了问题。