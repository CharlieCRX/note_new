# DDS多通道功放控制系统接口设计 V2.3

# 1. 概述

## 1.1 功能说明

系统采用单路 AD5932 DDS 信号源，通过功放板内部通道切换电路实现最多12路输出。

支持：

- 固定频率输出
- 扫频输出
- 多通道轮询
- 定时运行
- 自动停止
- UDP JSON控制接口

------

## 1.2 系统架构

```text
+----------------+
| Qt Front End   |
+--------+-------+
         |
         | UDP JSON
         |
+--------v-------+
| DDS Server     |
+--------+-------+
         |
         |
+--------v-------+
| DDS Solver     |
+--------+-------+
         |
         |
+--------v-------+
| AD5932 Driver  |
+----------------+
```

DDS Solver负责将用户配置转换为AD5932寄存器参数。

------

# 2. DDS Solver策略

系统内部固定：

```text
SweepPoints = 100
```

即：

```text
NumIncrement = 99
```

用户无需配置：

- Delta Frequency
- Num Increment
- Interval Cycle

全部由系统自动求解。

------

# 3. 通信协议

统一格式：

```json
{
    "cmd":"xxx",
    "code":1,
    "data":{}
}
```

## 3.1 通用字段说明

| 字段 | 类型   | 说明       |
| ---- | ------ | ---------- |
| cmd  | string | 命令名称   |
| code | uint32 | 请求流水号 |
| data | object | 数据体     |

------

# 4. DDS配置接口

## 4.1 固定频率模式

### 请求

```json
{
    "cmd":"dds_config",
    "code":1,
    "data":
    {
        "signal":
        {
            "mode":"fixed",
            "frequencyHz":10000,
            "durationMs":2
        },

        "scheduler":
        {
            "channels":[1,2,3,4,5,6,7,8,9,10],

            "channelPeriodMs":500,

            "runHours":2,

            "autoStopMode":"after_channel"
        }
    }
}
```

### signal字段说明

| 字段        | 类型   | 单位 | 说明                         |
| ----------- | ------ | ---- | ---------------------------- |
| mode        | string | -    | 固定频率模式，固定值为 fixed |
| frequencyHz | uint32 | Hz   | DDS输出频率                  |
| durationMs  | double | ms   | 单次DDS输出持续时间          |

------

## 4.2 扫频模式

### 请求

```json
{
    "cmd":"dds_config",
    "code":2,
    "data":
    {
        "signal":
        {
            "mode":"sweep",

            "startFreqHz":1000,

            "stopFreqHz":10000,

            "durationMs":2
        },

        "scheduler":
        {
            "channels":[1,2,3,4,5,6,7,8,9,10],

            "channelPeriodMs":500,

            "runHours":2,

            "autoStopMode":"after_channel"
        }
    }
}
```

### sweep字段说明

| 字段        | 类型   | 单位 | 说明                     |
| ----------- | ------ | ---- | ------------------------ |
| mode        | string | -    | 扫频模式，固定值为 sweep |
| startFreqHz | uint32 | Hz   | 起始频率                 |
| stopFreqHz  | uint32 | Hz   | 终止频率                 |
| durationMs  | double | ms   | 单次扫频持续时间         |

------

## 4.3 scheduler字段说明

| 字段            | 类型   | 单位 | 说明                           |
| --------------- | ------ | ---- | ------------------------------ |
| channels        | array  | -    | 参与轮询的通道编号             |
| channelPeriodMs | uint32 | ms   | 同一通道两次输出之间的时间间隔 |
| runHours        | double | h    | 总运行时间                     |
| autoStopMode    | string | -    | 自动停止策略                   |

------

## 4.4 channels说明

支持范围：

```text
1 ~ 12
```

例如：

```json
{
    "channels":[1,3,5,7]
}
```

轮询顺序：

```text
CH1
↓
CH3
↓
CH5
↓
CH7
↓
CH1
...
```

------

## 4.5 runHours说明

### 无限运行

```json
{
    "runHours":0
}
```

表示：

```text
无限运行
```

必须由用户发送：

```text
dds_stop
```

停止任务。

------

### 定时运行

```json
{
    "runHours":2
}
```

表示：

```text
运行2小时
```

达到时间后自动执行停止策略。

------

## 4.6 autoStopMode说明

支持：

| 参数          | 说明               |
| ------------- | ------------------ |
| after_channel | 当前通道完成后停止 |
| after_round   | 当前轮完成后停止   |
| immediately   | 立即停止           |

推荐：

```text
after_channel
```

------

# 5. DDS配置返回

## 成功

```json
{
    "cmd":"dds_config_ack",

    "code":2, // 在定频模式下，code = 1

    "result":"ok",

    "resolved":
    {
        "sweepPoints":100,

        "deltaFreqHz":91,

        "numIncrements":99,

        "intervalCycles":3,

        "actualDurationUs":1987,

        "errorUs":13
    }
}
```

### resolved字段说明

| 字段             | 说明               |
| ---------------- | ------------------ |
| sweepPoints      | 实际扫频点数       |
| deltaFreqHz      | 求解后的频率步长   |
| numIncrements    | AD5932频率递增次数 |
| intervalCycles   | AD5932周期参数     |
| actualDurationUs | 实际输出时间       |
| errorUs          | 时间误差           |

------

## 失败

```json
{
    "cmd":"dds_config_ack",

    "code":2,

    "result":"error",

    "errorCode":1001,

    "message":"unable to solve DDS parameters"
}
```

------

# 6. 启动任务

## 请求

```json
{
    "cmd":"dds_start",
    "code":3
}
```

## 响应

```json
{
    "cmd":"dds_start_ack",

    "code":3,

    "result":"ok"
}
```

------

# 7. 停止任务

## 请求

```json
{
    "cmd":"dds_stop",

    "code":4,

    "data":
    {
        "mode":"after_channel"
    }
}
```

### mode字段说明

| 参数          | 说明               |
| ------------- | ------------------ |
| after_channel | 当前通道完成后停止 |
| after_round   | 当前轮完成后停止   |
| immediately   | 立即停止           |

------

# 8. 状态查询

## 请求

```json
{
    "cmd":"dds_get_status",
    "code":5
}
```

## 返回

```json
{
    "cmd":"dds_status",

    "code":5,

    "data":
    {
        "running":true,

        "currentChannel":4,

        "currentRound":12345,

        "uptimeHours":1.5,

        "remainingHours":0.5
    }
}
```

### 字段说明

| 字段           | 说明         |
| -------------- | ------------ |
| running        | 当前是否运行 |
| currentChannel | 当前输出通道 |
| currentRound   | 已完成轮数   |
| uptimeHours    | 已运行时间   |
| remainingHours | 剩余时间     |

------

# 9. 统计信息

## 请求

```json
{
    "cmd":"dds_get_statistics",
    "code":6
}
```

## 返回

```json
{
    "cmd":"dds_statistics",

    "code":6,

    "data":
    {
        "totalRunHours":456.8,

        "totalRounds":456789,

        "completedTasks":123,

        "errorCount":0
    }
}
```

### 字段说明

| 字段           | 说明         |
| -------------- | ------------ |
| totalRunHours  | 累计运行时间 |
| totalRounds    | 累计轮数     |
| completedTasks | 已完成任务数 |
| errorCount     | 错误次数     |

------

# 10. DDS能力查询

## 请求

```json
{
    "cmd":"dds_get_capability",
    "code":10
}
```

## 返回

```json
{
    "cmd":"dds_capability",

    "code":10,

    "data":
    {
        "sweepPoints":100,

        "maxChannels":12,

        "channelSwitchDelayMs":10,

        "minFrequencyHz":1000,

        "maxFrequencyHz":5000000,

        "maxSignalDurationMs":1000,

        "maxRunHours":168
    }
}
```

### 字段说明

| 字段                 | 说明             |
| -------------------- | ---------------- |
| sweepPoints          | 固定扫频点数     |
| maxChannels          | 最大通道数量     |
| channelSwitchDelayMs | 通道切换时间     |
| minFrequencyHz       | 最小支持频率     |
| maxFrequencyHz       | 最大支持频率     |
| maxSignalDurationMs  | 最大单次信号时间 |
| maxRunHours          | 最大任务运行时间 |

------

# 11. 异步事件

## DDS启动

```json
{
    "event":"dds_started"
}
```

------

## DDS停止

```json
{
    "event":"dds_stopped"
}
```

------

## DDS异常

```json
{
    "event":"dds_error",

    "data":
    {
        "code":1001,

        "message":"unable to solve DDS parameters"
    }
}
```

------

## 任务完成

```json
{
    "event":"dds_finished",

    "data":
    {
        "reason":"duration_reached",

        "runHours":2
    }
}
```

表示：

```text
达到设定运行时间
并已按照自动停止策略完成停止
```

------

# 12. 调度约束

设：

```text
N = channels.size()

P = channelPeriodMs

W = signal.durationMs
```

则：

```text
Tswitch = P / N
```

必须满足：

```text
W + channelSwitchDelayMs < Tswitch
```

否则拒绝启动。

例如：

```text
channels = [1,2,3,4,5,6,7,8,9,10]

N = 10

P = 500ms

Tswitch = 50ms
```

若：

```text
signal.durationMs = 2ms

channelSwitchDelayMs = 10ms
```

则：

```text
2 + 10 < 50
```

配置合法。

------

# 13. 自动停止策略

当：

```text
运行时间 >= runHours
```

系统自动执行：

```text
dds_stop(autoStopMode)
```

例如：

```text
runHours = 2

autoStopMode = after_channel
```

执行流程：

```text
运行2小时

↓

达到停止时间

↓

等待当前通道完成

↓

停止DDS

↓

发送 dds_finished

↓

发送 dds_stopped
```