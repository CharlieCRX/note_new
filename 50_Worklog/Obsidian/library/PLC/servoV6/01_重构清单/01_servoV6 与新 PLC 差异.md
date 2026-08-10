## 结论

建议在 `servoV6` 现有工程上进行“协议层保留、领域模型重构”，不建议从零重写。

但这不是简单改寄存器地址。当前 `servoV6` 的通信框架值得保留，而轴模型、联动模型、启动流程和状态解释需要成体系替换。

粗略估算：

|方案|预计投入|风险|
|---|---|---|
|基于 servoV6 重构|35–55 人日|中等，可分阶段验证|
|从零重新开发|70–105 人日|高，通信、状态机、Android、摇杆和异常处理都要重新验证|

重构成本大约是重写的 50%–65%。当前项目已经有约 1.7 万行生产代码、1.7 万行测试代码，以及完整的 Modbus、日志、断线重连和测试框架，因此从零重写的性价比不高。

---

## 一、servoV6 与新 PLC 的根本差异

### 1. 轴模型已经不匹配

当前 `servoV6` 固定使用六种轴：

```
enum class AxisId {
    Y, Z, R, X, X1, X2
};
```

见 [AxisId.h](F:\\project\\servoV6\\domain\\entity\\AxisId.h)。

新 PLC 是统一的 16 轴数组模型：

|PLC 索引|对象|
|---|---|
|0–9|L0–L9 实体轴|
|10–12|C0–C2 实体轴|
|13–15|SYN0–SYN2 联动逻辑轴|

因此不能继续通过 `switch (AxisId::X/Y/Z...)` 选择寄存器。应改成：

```
struct AxisKey {
    uint8_t plcIndex;       // 0..15
    std::string name;       // 用户配置名称
    AxisKind kind;          // Physical / Coupled
    EngineeringUnit unit;   // mm / deg
};
```

界面显示名称与 PLC 数组下标应分离。例如用户可以把 PLC 轴 3 显示成“左升降轴”，但底层仍使用索引 3。

---

### 2. 旧版龙门模型不再适用

servoV6 当前把联动写死为：

- X：逻辑龙门轴
- X1、X2：两根物理轴
- `GantryCouplingController`
- `GantryPowerController`
- 专用的 `LINKAGE_ENABLE`
- 专用的 `GANTRY_ERROR_CODE`

见 [GantryCouplingController.h](F:\\project\\servoV6\\domain\\gantry\\GantryCouplingController.h)。

新 PLC 则是通用联动模型：

- SYN0、SYN1、SYN2 三个联动轴；
- 每个联动轴包含一个主动成员和一个从动成员；
- 成员角色为 `1=主动，-1=从动`；
- 成员写入 `联动成员信息`；
- 联动结果由 `联动轴信息`反馈；
- 联动运行超差使用独立阈值和告警位。

因此建议删除“龙门专用领域概念”，改为通用模型：

```
struct LinkGroupConfig {
    uint8_t synAxisIndex;     // 13..15
    uint8_t masterAxisIndex;
    uint8_t slaveAxisIndex;
    double tolerance;
    bool enabled;
};
```

并新增：

- `LinkGroup`
- `LinkConfigurationService`
- `LinkState`
- `ApplyLinkConfigurationUseCase`
- `ReleaseLinkUseCase`

这样以后不论它是龙门、同步升降还是反向平移，servoV6 都不需要再增加专用代码。

需要特别注意：当前 PLC 的 `联动成员信息`只有 10 个成员，意味着至少从当前定义看，可能只有 L0–L9 能作为联动成员，C0–C2 是否可以加入需要 PLC 侧再次确认。

---

## 二、当前底层驱动与新 PLC 的具体冲突

### 1. 寄存器地址全部是旧协议

当前地址表仍是旧版散列布局，例如：

- X 电机使能：M0
- X 联动：M4
- X 绝对定位：M40
- Y 绝对定位：M42
- 设备急停：M80
- 急停反馈：M130
- 龙门错误码：D180

见 [RegisterAddressAll.h](F:\\project\\servoV6\\infrastructure\\plc\\protocol\\RegisterAddressAll.h)。

新 PLC 已改成连续数组布局：

|新 PLC 功能|地址计算|
|---|---|
|轴控入口|`M0 + i`|
|相对原点清除|`M16 + i`|
|绝对位置清零|`M32 + i`|
|绝对定位触发|`M48 + i`|
|相对定位触发|`M64 + i`|
|点动正转|`M80 + i`|
|点动反转|`M96 + i`|
|报警解除|`M112 + i`|
|电机使能|`M128 + i`|
|相对定位终止|`M144 + i`|
|绝对定位终止|`M160 + i`|
|相对原点设置|`M176 + i`|
|点动心跳|`M192 + i`|
|告警清零|`M208 + i`|
|全局急停|`M224`|
|急停解除|`M225`|

D 寄存器也是数组计算：

```
手动速度       D1000 + i×2
定位速度       D1032 + i×2
相对原点记录   D1064 + i×2
绝对定位目标   D1096 + i×2
相对定位距离   D1128 + i×2
软件负限位     D1160 + i×2
软件正限位     D1192 + i×2
```

新的地址实现不应再声明几百个 `constexpr RegisterInfo`，而应使用地址公式：

```
RegisterInfo NewPlcMap::motorEnable(uint8_t i) {
    return coil(128 + i);
}

RegisterInfo NewPlcMap::absoluteTarget(uint8_t i) {
    return float32Holding(1096 + i * 2);
}
```

---

### 2. 急停协议发生了根本变化

当前 servoV6 使用：

```
EmergencyStopCommand {
    bool active;
};
```

`true` 和 `false` 都写入同一个急停寄存器。

新 PLC 是两个独立命令：

- `M224`：设备急停
- `M225`：设备急停解除

所以应拆分成：

```
struct TriggerEmergencyStopCommand {};
struct ReleaseEmergencyStopCommand {};
```

急停解除不能再通过向急停寄存器写 `false` 实现。

另外，新 PLC 没有明确的“急停状态反馈 M130”。暂时可以将 M224 作为锁存状态读取，但最好让 PLC 新增一个只读的系统状态字段，明确区分：

- 急停请求；
- 急停执行中；
- 急停锁存；
- 急停解除允许。

---

### 3. 状态解释完全不同

servoV6 当前读取旧版的：

- 状态寄存器；
- 绝对定位中；
- 相对定位中；
- 点动中；
- 报警码；

再通过 `AxisStateDeriver` 合成状态。

新 PLC 已经直接发布：

```
运动状态：
0 未使能
1 使能静止
2 正向点动
3 反向点动
4 绝对定位
5 相对定位
```

以及：

```
限位状态：
0 无
1 软件正限位
2 软件负限位
3 硬件正限位
4 硬件负限位
```

告警改成位域：

```
bit0 急停
bit1 联动角色占用
bit2 联动位置超差/加入失败
bit3 联动成员配置错误
bit4 联动运行超差
bit5 轴或功能块错误
bit6 点动心跳超时
```

因此：

- `AxisStateDeriver` 应删除或大幅简化；
- 不再使用 `alarmCode == 3` 推断软限位；
- 告警应按位解析；
- 正向点动和反向点动应保留为两个领域状态，或者在 `AxisFeedback` 中增加方向；
- 状态、限位、告警的实际 D 地址需要 PLC 再导出一份正式点表。

---

### 4. 点动心跳是新协议新增的强制要求

新 PLC 点动期间要求持续刷新 `M192+i`，3 秒超时后自动停止。

这不能依赖 QML 按钮事件或主界面刷新，应由基础设施层独立调度：

```
class JogHeartbeatService {
public:
    void start(uint8_t axis);
    void stop(uint8_t axis);
    void tick();
};
```

建议：

- 200–500 ms 发送一次；
- 只对正在点动的轴发送；
- 断线、急停、页面切换、应用进入后台时立即停止点动；
- Android 进入后台时应主动清除所有点动位；
- 不能等 3 秒 PLC 超时才作为正常停止手段。

---

### 5. 通用 StopCommand 已经不够准确

新 PLC 的停止方式依命令类型不同：

- 点动停止：将对应点动位写 `false`；
- 绝对定位停止：触发 `M160+i`；
- 相对定位停止：触发 `M144+i`；
- 全局急停：M224。

所以建议将 `StopCommand` 改成明确语义：

```
struct StopJogCommand {};
struct AbortAbsoluteMoveCommand {};
struct AbortRelativeMoveCommand {};
struct EmergencyStopCommand {};
```

或者由驱动根据最后一个可信运动状态选择停止寄存器，但我更建议由应用层明确发出停止类型，避免状态反馈延迟导致写错寄存器。

---

## 三、配置界面不能只是几个复选框

你提出的启动前配置界面是必须的，但它应该被设计成完整的“系统投运流程”。

### 建议启动状态机

```
Disconnected
  → Connected
  → ProtocolVerified
  → ConfigurationRequired
  → ApplyingConfiguration
  → VerifyingConfiguration
  → Ready
  → Running
```

只有进入 `Ready` 后，才允许：

- 电机使能；
- 点动；
- 定位；
- UDP 远程命令；
- 摇杆控制。

### 配置内容至少包括

#### 轴配置

- PLC 索引 0–15；
- 显示名称；
- 是否启用轴控入口 `M0+i`；
- 轴类型：实体轴/联动轴；
- 单位：mm/deg；
- 正负软限位；
- 软限位启用位；
- 默认点动速度；
- 默认定位速度；
- 是否允许摇杆/UDP 控制；
- 所属设备或分组。

#### 联动配置

- SYN0/SYN1/SYN2 是否使用；
- 主动轴；
- 从动轴；
- 主从方向；
- 超差阈值；
- 联动轴是否启用；
- 配置有效性检查。

### 推荐的配置下发顺序

1. 建立 PLC 连接并校验协议版本。
2. 禁止所有运动入口。
3. 清除残留点动、定位和终止命令。
4. 确认所有相关轴静止。
5. 关闭相关成员电机使能。
6. 清除 PLC 现有联动成员配置。
7. 写入主、从成员结构体。
8. 写入超差阈值。
9. 写入软限位和默认速度。
10. 设置实体轴与 SYN 轴的 `使能轴控`。
11. 读取 `联动轴信息`进行回读确认。
12. 所有配置一致后进入 `Ready`。

配置写入必须使用“写入—回读—确认”，不能只根据 Modbus 写成功就认为配置已生效。

---

## 四、建议的 servoV6 新架构

### 1. 保留的部分

这些模块复用价值很高：

- `AsioModbusTcpClient`
- Modbus TCP 帧处理
- `PlcDevice`
- `PlcPoller`
- `PlcSnapshot`
- `RegisterCodec`
- 端序处理
- 连接状态和自动重连
- 日志系统
- QML 基础组件
- FakePLC 测试思想
- 命令/反馈分离模式
- ViewModel 与领域层隔离方式

### 2. 需要替换的部分

基本需要重做：

- `RegisterAddressAll.h`
- 当前 `ModbusSystemDriver` 的寄存器选择器
- `AxisId` 六轴枚举
- `SystemContext` 固定创建六轴的逻辑
- `GantryCouplingController`
- `GantryPowerController`
- `GantryViewModel`
- `Machine_A/Machine_B` 硬编码
- QML 中 X/X1/X2/Y/Z/R 的显式分支
- 急停寄存器处理
- 旧版状态推导
- R 轴软限位使用 X1 地址的占位逻辑

当前 `SystemContext` 在构造时固定创建 6 个轴，见 [SystemContext.h](F:\\project\\servoV6\\domain\\entity\\SystemContext.h)。建议改成从配置动态构建：

```
class MachineContext {
public:
    void applyTopology(const MachineConfiguration&);
    Axis* axis(uint8_t plcIndex);
    LinkGroup* link(uint8_t synIndex);
};
```

### 3. 驱动应拆成三层

```
ModbusTransport
    只负责 TCP、帧、重连

NewPlcProtocol
    负责地址公式、数据类型、批量读写、告警位解析

MotionSystemDriver
    负责领域命令 ↔ PLC 协议转换
```

不要继续把地址选择、状态推导、边沿调度、联动和领域反馈都集中在一个近千行的 `ModbusSystemDriver.h` 中。

---

## 五、还需要补充的关键机制

### 1. PLC 协议版本识别

强烈建议 PLC 增加：

```
协议版本
程序版本
配置结构版本
轴数量
联动轴数量
```

servoV6 连接后先校验版本。不匹配时只允许查看原始状态，禁止运动。

否则以后 PLC 再改一次地址，servoV6 仍然可能连接成功但控制错误的轴，这是最危险的故障类型。

---

### 2. 配置漂移检测

上位机保存：

- 期望配置；
- PLC 当前配置；
- 上次成功下发的配置；
- 配置校验值或版本。

启动时显示差异：

```
本地配置：L0 + L1 → SYN0
PLC 当前：L2 + L3 → SYN0
状态：不一致，禁止运行
```

不能每次启动都无条件覆盖 PLC，也不能默认相信 PLC 当前配置。

---

### 3. 中央命令仲裁器

当前项目已经出现过运动资源互斥和按钮锁定问题。新版本建议增加统一仲裁：

```
class MotionCommandArbiter {
    // 每轴同一时刻只允许一个运动所有者
};
```

所有命令来源都必须经过它：

- QML 按钮；
- 摇杆；
- UDP；
- 自动流程；
- 联动操作；
- 急停恢复。

仲裁器需要解决：

- 点动与定位互斥；
- 同一实体轴不能加入多个 SYN；
- 联动成员轴禁止独立运动；
- 配置阶段禁止运行；
- 急停状态禁止运行；
- 断线状态禁止运行；
- 命令超时后释放所有权。

---

### 4. 通信轮询分级

不建议所有寄存器都以 10 ms 读取。

建议：

|数据|周期|
|---|---|
|运动状态、位置、急停、点动心跳|20–50 ms|
|告警、限位|50–100 ms|
|速度参数、软限位|200–500 ms|
|配置结构、联动成员|配置时读取或 1 s|
|诊断和版本|启动时/手动刷新|

新 PLC 地址连续，非常适合批量读取。驱动应按连续区间构建读计划，而不是逐变量读写。

---

### 5. 原始诊断页面

建议配置页面增加“PLC 诊断”标签，能够查看：

- M0–M225 原始位；
- D 地址原始值；
- 当前轴索引；
- 运动状态、限位状态；
- 告警位解析；
- 联动成员结构；
- 最近一次命令；
- 最后一次 Modbus 异常；
- 点动心跳发送时间；
- PLC/上位机协议版本。

这会显著降低现场调试成本。

---

## 六、推荐的实施步骤

### 阶段 1：冻结新 PLC 接口，3–5 人日

先形成正式 ICD 点表，补齐：

- 状态变量地址；
- 结构体内存布局；
- REAL 字序；
- 联动成员可用范围；
- 告警位定义；
- 急停状态反馈；
- PLC 协议版本。

没有这份契约，不建议直接开始改 UI。

### 阶段 2：实现 NewPlcProtocol 和 FakePLC，5–8 人日

先不动界面，实现：

- 数组地址计算；
- 16 轴反馈读取；
- 告警/限位解析；
- 心跳；
- 急停与解除；
- 联动配置写入与回读；
- FakePLC 新协议模拟。

### 阶段 3：动态轴领域模型，5–8 人日

替换固定 `AxisId` 和 Gantry 模型，引入：

- 动态轴；
- 通用 SYN 联动；
- 配置状态机；
- 命令仲裁器。

### 阶段 4：应用层与驱动接入，6–10 人日

重构：

- `SystemCommand`
- `ModbusSystemDriver`
- 各 UseCase
- 点动心跳
- 定位终止
- 急停恢复
- 联动建立与解除。

### 阶段 5：配置 UI 和动态主界面，5–8 人日

实现：

- 启动配置向导；
- 动态轴列表；
- 联动编辑器；
- 配置回读；
- Ready 门禁；
- 诊断页面。

### 阶段 6：测试与现场调试，10–16 人日

至少验证：

- 16 个索引不会串轴；
- 配置失败不进入运行；
- 联动成员重复；
- 联动位置超差；
- 点动断网停止；
- Android 进入后台停止；
- 急停后不自动续跑；
- PLC 重启后的配置漂移；
- 上位机重启时 PLC 仍在使能状态；
- Modbus 写成功但 PLC 拒绝命令；
- 正反软硬限位。

---

## 七、重构还是重写

### 重构更合适的原因

当前项目虽然业务模型已经过时，但底层工程能力并不差：

- 已采用 Clean Architecture；
- 有命令/反馈分离；
- 有 Modbus 协议运行时；
- 有端序处理；
- 有断线重连；
- 有日志和链路追踪；
- 有 FakePLC；
- 有 47 个测试文件；
- Qt/QML、Android、摇杆和 UDP 已经打通。

从零重写意味着这些非业务问题全部重新踩一遍。

### 重构的实际复用率

|模块|预计复用率|
|---|---|
|Modbus TCP 传输、编码、轮询|80%–90%|
|日志、连接管理|80%–90%|
|QML 通用组件|60%–80%|
|Axis 领域实体思想|40%–60%|
|UseCase/ViewModel 模式|40%–60%|
|寄存器地址与状态推导|0%–20%|
|Gantry 专用逻辑|0%–20%|
|启动装配与固定轴绑定|0%–20%|

综合看，可以复用约 45%–60% 的有效资产。

### 什么时候才应该重写

只有在以下条件同时出现时，才建议从零开发：

- 现有 Qt/QML UI 也准备完全推翻；
- 不再支持 Android、摇杆或 UDP；
- 通信协议不再使用 Modbus TCP；
- 设备分组和操作流程完全改变；
- 当前测试无法运行且代码长期无人能维护；
- 团队准备接受较长的现场重新验证周期。

从当前代码情况看，这些条件并不成立。

---

## 最终建议

不要在当前代码上直接把旧地址替换成新地址，也不要从零重写。

最合适的路线是：

> 保留 Modbus/Qt/日志/测试基础设施，新建 `NewPlcProtocol`，用动态 16 轴模型替换固定六轴模型，用通用 SYN 联动替换 Gantry 专用模型，并增加“配置—校验—Ready—运行”的启动生命周期。

第一个实际产出不应是配置页面，而应是“新 PLC 上位机接口契约表”。接口冻结后，再从 FakePLC 和协议驱动开始改，最后接 UI。这样最容易控制风险，也能在每一阶段保持可测试、可回退。