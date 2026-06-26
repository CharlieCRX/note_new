# 1. 分析目标

验证 SSD2351 平台是否真正支持 **I2S0 6-Wire** 工作模式，并确认 TX、RX 两条数据通路能够同时正常工作。

需要回答的问题包括：

1. PCB 是否已经按照 6-Wire 布线？
2. PadMux 是否已经切换到正确的 I2S 功能？
3. Device Tree 是否已经声明 6-Wire 工作模式？
4. Linux Driver 是否真正将 I2S Controller 配置成 6-Wire？

------

# 2. 当前已经完成的验证

## 2.1 PCB 管脚定义确认

依据：

《USBL 水面设备管脚定义及器件地址说明》

确认 PCB 上定义了如下 I2S0 信号：

| I2S 信号 | PAD          |
| -------- | ------------ |
| TX_BCLK  | PAD_GPIOD_04 |
| TX_WCK   | PAD_GPIOD_05 |
| TX_SDO0  | PAD_GPIOD_03 |
| RX_BCLK  | PAD_OUTP_CH2 |
| RX_WCK   | PAD_OUTN_CH2 |
| RX_SDI   | PAD_OUTP_CH3 |

说明：

PCB 已经将 TX 与 RX 分别布线，符合 6-Wire 硬件连接方式。

------

## 2.2 ARMTmux 功能确认

分析 ARMTmux 后发现：

每个 PAD 都存在：

- PAD_Name（硬件名称）
- SW_Name（软件名称）

例如：

| PAD_Name     | SW_Name       |
| ------------ | ------------- |
| PAD_GPIOD_04 | PAD_PWM_ADC00 |
| PAD_GPIOD_05 | PAD_PWM_ADC01 |

因此：

```text
PAD_GPIOD_04
        │
        └──── SW_Name：PAD_PWM_ADC00
PAD_GPIOD_05
        │
        └──── SW_Name：PAD_PWM_ADC01
```

说明：

USBL 文档采用的是 **PAD_Name**；

而 Linux DTS 使用的是 **SW_Name**。

二者实际上引用的是同一颗物理 PAD。

------

## 2.3 PadMux 配置确认

结合：

```
pcupid-ssm001c-s01a-voip-padmux.dtsi
```

可以确认：

| DTS 使用 SW_Name | 实际 Hardware PAD | I2S 功能 |
| ---------------- | ----------------- | -------- |
| PAD_PWM_ADC00    | PAD_GPIOD_04      | TX_BCK   |
| PAD_PWM_ADC01    | PAD_GPIOD_05      | TX_WCK   |
| PAD_GPIOD_03     | PAD_GPIOD_03      | TX_SDO0  |
| PAD_OUTP_CH2     | PAD_OUTP_CH2      | RX_BCK   |
| PAD_OUTN_CH2     | PAD_OUTN_CH2      | RX_WCK   |
| PAD_OUTP_CH3     | PAD_OUTP_CH3      | RX_SDI   |

因此：

PadMux 配置与 USBL 管脚定义实际上是一致的。

不存在真正的 PAD 配置错误。

------

## 2.4 Device Tree 配置确认

在：

```
pcupid.dtsi
```

中已经发现：

```dts
i2s-trx-shared-padmux = <0>;
i2s-tx-padmux = <6>;
i2s-rx-padmux = <7>;

i2s-tx0-tdm-wiremode = <2>;
i2s-rx0-tdm-wiremode = <2>;
```

根据厂商注释：

```
wiremode = 2
```

表示：

```
6 Wire
```

因此：

Device Tree 已经明确声明：

I2S0 工作于：

- TX 6-Wire
- RX 6-Wire

模式。

------

# 3. 当前能够得出的结论

截至目前，可以确认以下几点。

| 验证项        | 结果     | 说明                                   |
| ------------- | -------- | -------------------------------------- |
| PCB 布线      | ✔ 已确认 | TX、RX 已分别布线，符合 6-Wire。       |
| USBL 管脚定义 | ✔ 已确认 | 已定义完整的 TX、RX PAD。              |
| ARMTmux       | ✔ 已确认 | 已建立 PAD_Name ↔ SW_Name 的映射关系。 |
| PadMux DTS    | ✔ 已确认 | 实际配置与 PCB 一致。                  |
| Device Tree   | ✔ 已确认 | 已声明 6-Wire 工作模式。               |

截至目前：

**没有发现 PadMux 配置错误。**

目前已经能够确认：

> PCB → ARMTmux → PadMux DTS

这一部分配置链已经闭环。

------

# 4. 当前仍然无法证明的问题

虽然：

PadMux 已经正确。

但是：

目前仍不能证明：

**I2S Controller 已真正工作于 6-Wire。**

原因是：

PadMux 仅负责：

```
PAD

↓

I2S 功能
```

它并不负责：

```
TX_BCLK

是否

与

RX_BCLK

共享。
```

真正控制：

- TX/RX 是否独立
- Master / Slave
- WireMode
- TDM

的是：

```
I2S Controller
```

因此：

真正决定：

6-Wire 是否生效的是：

```
sstar_bach Driver
```

而不是：

PadMux。

------

# 5. 下一阶段分析目标

下一阶段建议开始分析：

```
Device Tree
        │
        ▼
sstar_bach Driver
        │
        ▼
I2S Controller Register
```

建议按照下面顺序进行验证。

------

## 第一步：分析 sstar_bach Probe

确认：

Driver 是否读取：

```dts
i2s-tx-padmux

i2s-rx-padmux

i2s-tx0-tdm-wiremode

i2s-rx0-tdm-wiremode
```

需要重点查找：

```
of_property_read_u32()

of_property_read_bool()
```

读取哪些 Device Tree 属性。

目标：

建立：

```
DTS

↓

Driver
```

对应关系。

------

## 第二步：确认 Driver 如何配置 I2S Controller

继续分析：

```
wiremode
```

最终写入：

哪些：

```
I2S Register
```

重点关注：

是否存在：

```
switch(wiremode)

case 4 wire

case 6 wire
```

或者：

```
REG_I2S_MODE

REG_I2S_CTRL

REG_TDM_CFG
```

等寄存器配置。

目标：

证明：

Driver 是否真正开启：

TX_BCLK

RX_BCLK

独立工作。

------

## 第三步：确认 TX、RX 是否同时初始化

继续分析：

Driver 是否：

分别初始化：

```
TX DMA

RX DMA
```

确认：

TX

RX

是否：

都注册到：

ALSA。

重点关注：

```
Playback

Capture
```

PCM 是否同时存在。

------

## 第四步：分析 sstar_pcm

确认：

DMA 是否分别建立：

```
Playback DMA

Capture DMA
```

验证：

TX

RX

是否真正拥有：

独立 DMA 通道。

------

## 第五步：结合 acm8816Launch 验证

最后：

利用：

```
acm8816Launch
```

分别验证：

- TX 是否正常发送；
- RX 是否正常接收；
- TX 与 RX 是否能够同时工作。

最终：

通过：

示波器 +

ALSA +

DMA +

Codec

共同验证：

6-Wire 是否真正生效。

------

# 6. 后续完整分析路线

整个验证过程建议按照如下顺序逐层推进。

```text
PCB（USBL 管脚定义）
        │
        ▼
ARMTmux
        │
        ▼
PadMux DTS
        │
        ▼
PadMux Driver
        │
        ▼
I2S Controller Driver（sstar_bach）
        │
        ▼
I2S Register
        │
        ▼
PCM Driver（sstar_pcm）
        │
        ▼
DMA
        │
        ▼
ALSA PCM
        │
        ▼
acm8816Launch
        │
        ▼
示波器验证
```

上述链路中，前 **PCB → Device Tree(PadMux)** 已基本验证完成。

下一阶段工作的重点将转移到：

**Driver 是否真正按照 Device Tree 的配置，将 I2S Controller 初始化为 6-Wire 工作模式，并最终实现 TX 与 RX 同时工作的完整验证。**