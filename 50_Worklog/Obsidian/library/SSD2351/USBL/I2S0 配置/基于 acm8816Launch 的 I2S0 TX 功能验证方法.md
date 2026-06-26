# 1. 验证目标

本阶段验证目标为：

> 验证 SSD2351 的 I2S0 TX 通路是否已经真正建立。

这里所说的 **TX 工作正常**，并不仅仅表示：

- acm8816Launch 能够运行；
- ALSA 没有报错；

而是需要证明：

```text
acm8816Launch
        │
        ▼
ALSA PCM
        │
        ▼
DMA
        │
        ▼
I2S Controller
        │
        ▼
PadMux
        │
        ▼
PCB
```

整个 TX 数据链路均已经正常工作。

因此，本次验证重点放在：

**I2S 实际输出信号是否正确。**

------

# 2. 为什么选择 acm8816Launch？

目前系统已经能够运行：

```text
acm8816Launch
```

该程序能够通过 ALSA Playback 接口持续向 I2S0 发送 PCM 数据。

因此：

它可以作为：

**I2S TX 激励源（Stimulus Source）**。

相比直接播放音乐：

建议发送：

- 固定频率正弦波（例如 1kHz）
- 固定采样率（例如 48kHz）
- 固定位宽（16bit 或 24bit）

这样：

I2S 总线上的时钟频率具有确定性，更容易利用示波器进行验证。

------

# 3. 为什么要测量 MCLK、BCLK、LRCK、SDO？

I2S TX 输出包含四个最重要的信号。

| 信号        | 作用            | 是否必须 |
| ----------- | --------------- | -------- |
| MCLK        | 主时钟          | 建议测量 |
| BCLK        | Bit Clock       | 必须     |
| LRCK（WCK） | Word Clock      | 必须     |
| SDO         | Serial Data Out | 必须     |

只有这四个信号全部正确，才能说明：

I2S TX 已真正开始发送数据。

------

# 4. 每个信号代表什么？

## 4.1 MCLK（Master Clock）

MCLK 是 Codec 的工作参考时钟。

例如：

```text
24.576 MHz
```

或者：

```text
12.288 MHz
```

具体频率取决于：

- PLL
- Codec 配置
- Device Tree

如果：

MCLK 没有输出，

说明：

I2S Controller 很可能没有启动。

因此：

MCLK 可以作为：

**I2S 是否启动的第一判断依据。**

------

## 4.2 LRCK（Word Clock）

LRCK（也称：

- WCK
- WS
- Frame Sync）

用于表示：

**当前正在发送哪一个采样点。**

例如：

左右声道：

```text
L
R
L
R
L
R
```

LRCK 每变化一次：

表示：

开始发送：

新的采样数据。

因此：

LRCK 的频率：

永远等于：

**采样率（Sample Rate）。**

例如：

当前：

```text
48kHz
```

播放：

那么：

理论：

```text
LRCK = 48kHz
```

如果：

播放：

```text
96kHz
```

则：

理论：

```text
LRCK = 96kHz
```

因此：

测量：

LRCK

即可立即判断：

I2S 当前工作的采样率是否正确。

------

# 5. 为什么 LRCK≈48kHz？

假设：

acm8816Launch 配置：

```text
Sample Rate = 48000Hz
```

意味着：

Codec：

每秒：

需要：

48000 个采样点。

I2S：

每发送完：

一个采样点，

LRCK：

翻转一次。

因此：

每秒：

完成：

48000 次采样。

所以：

```text
LRCK = Sample Rate
```

即：

```text
LRCK = 48000Hz
```

这是：

I2S 协议决定的，

与：

CPU

无关。

因此：

示波器测得：

约：

```text
48kHz
```

即可证明：

I2S 当前工作在：

48kHz。

------

# 6. 为什么 BCLK 是 3.072MHz？

BCLK：

表示：

**发送每一个 Bit 所需的时钟。**

因此：

BCLK：

取决于：

- Sample Rate
- 每个 Slot 位数
- Channel 数量

计算公式：

```text
BCLK = SampleRate × SlotWidth × ChannelCount
```

例如：

当前：

```text
48kHz

32bit Slot

Stereo
```

则：

```text
BCLK

=

48000

×

32

×

2

=

3072000Hz
```

即：

```text
3.072MHz
```

如果：

Slot 为：

16bit：

则：

```text
48000

×

16

×

2

=

1.536MHz
```

因此：

示波器测得：

BCLK：

可以立即判断：

当前：

Driver：

究竟采用：

16bit Slot

还是：

32bit Slot。

这也是验证 Driver 配置是否正确的重要依据。

------

# 7. SDO 为什么不能只看有没有波形？

很多人：

只看：

SDO：

是否跳变。

实际上：

这是不够的。

原因：

SDO：

发送的是：

PCM 数据。

如果：

发送：

全零：

那么：

SDO：

可能：

长期保持：

低电平。

但是：

I2S：

仍然：

工作正常。

因此：

判断：

SDO：

应结合：

播放内容。

例如：

播放：

1kHz 正弦波。

那么：

SDO：

应持续变化。

播放：

静音。

SDO：

可能：

几乎保持不变。

因此：

SDO：

只能作为：

辅助验证。

真正重要的是：

BCLK

与

LRCK。

------

# 8. 推荐验证步骤

## Step1

启动：

```text
acm8816Launch
```

播放：

```text
48kHz

16bit

Stereo

1kHz Sine
```

------

## Step2

利用示波器：

测量：

```text
MCLK
```

确认：

是否存在稳定输出。

------

## Step3

测量：

```text
TX_BCLK
```

确认：

频率是否符合：

理论计算。

例如：

约：

```text
3.072MHz
```

或者：

```text
1.536MHz
```

------

## Step4

测量：

```text
TX_WCK(LRCK)
```

确认：

频率是否约为：

```text
48kHz
```

如果：

播放：

96kHz。

则：

应约：

```text
96kHz
```

------

## Step5

测量：

```text
TX_SDO
```

确认：

存在持续数据输出。

如果：

播放：

正弦波，

应持续变化。

------

# 9. 验证通过标准

建议按照如下标准进行判断。

| 测量项 | 理论值                           | 验证目的             |
| ------ | -------------------------------- | -------------------- |
| MCLK   | 12.288MHz / 24.576MHz（依配置）  | Controller 是否启动  |
| LRCK   | ≈ SampleRate（例如48kHz）        | Sample Rate 是否正确 |
| BCLK   | SampleRate × SlotWidth × Channel | Driver 是否配置正确  |
| SDO    | 持续输出 PCM 数据                | TX 数据是否真正发送  |

------

# 10. 当前阶段能够证明什么？

如果：

以上：

四项：

全部符合理论值。

则可以证明：

```text
acm8816Launch
        │
        ▼
ALSA Playback
        │
        ▼
DMA
        │
        ▼
I2S Controller
        │
        ▼
PadMux
        │
        ▼
PCB TX
```

整个：

TX 数据链路已经完全打通。

这意味着：

SSD2351 已经具备：

**I2S0 TX 正常发送能力。**

但是：

**这仍然不能证明 SSD2351 已支持完整的 I2S0 6-Wire。**

因为：

目前仅验证了：

TX。

下一阶段仍需继续验证：

- RX 是否能够正常接收；
- TX 与 RX 是否能够同时工作；

只有：

TX

- 

RX

同时正常，

才能最终证明：

SSD2351 的 I2S0 已真正支持 **6-Wire 全双工模式**。