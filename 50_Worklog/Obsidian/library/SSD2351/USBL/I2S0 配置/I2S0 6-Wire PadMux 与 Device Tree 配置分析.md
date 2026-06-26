
# 1. 问题背景

目前需要验证 SSD2351 的 I2S0 是否真正工作在 **6-Wire** 模式。

目前已经掌握的资料包括：

- 《USBL 水面设备管脚定义及器件地址说明》
- 《SSD2351X_HW Checklist V1.03》中的 **ARMTmux** 页面
- Linux Device Tree
  - pcupid-ssm001c-s01a-voip-padmux.dtsi
  - pcupid.dtsi

容易产生疑问的是：

> padmux.dtsi 中大量使用的是 **PINMUX_FOR_I2S0_4W_TX_MODE_x**，为什么系统却配置成了 **6-Wire**？

本文分析这几个文件之间的关系，并说明它们共同决定 I2S0 工作模式的过程。

------

# 2. Linux 中 I2S 配置的整体流程

实际上，整个配置链如下：

```
PCB原理图
      │
      ▼
《USBL管脚定义》
      │
      ▼
ARMTmux(CheckList)
      │
      ▼
padmux.dtsi
      │
      ▼
PadMux Driver
      │
      ▼
PadMux Register
      │
      ▼
PAD切换成I2S功能
      │
      ▼
pcupid.dtsi
      │
      ▼
sstar_bach
      │
      ▼
I2S Controller Register
      │
      ▼
I2S开始收发数据
```

可以看到：

整个过程实际上可以拆成两部分：

**第一部分：PadMux**

负责：

> PAD 到底承担什么功能。

例如：

```
PAD_GPIOD_03
        │
        ▼
I2S0_TX_SDO
```

第二部分：

**I2S Controller**

负责：

> I2S 工作模式。

例如：

- Master / Slave
- 4 Wire / 6 Wire
- Channel 数量
- Slot 数量
- TDM

二者职责完全不同。

------

# 3. 《USBL管脚定义》负责什么？

例如：

| PAD          | 信号    |
| ------------ | ------- |
| PAD_GPIOD_03 | TX_SDO0 |
| PAD_OUTP_CH2 | RX_BCLK |
| PAD_OUTN_CH2 | RX_WCK  |
| PAD_OUTP_CH3 | RX_SDI  |

它描述的是：

```
CPU I2S0 TX_SDO
        │
        ▼
PAD_GPIOD_03
```

或者：

```
CPU RX_BCLK
      │
      ▼
PAD_OUTP_CH2
```

因此：

USBL 文档只是说明：

> **CPU 的 I2S 信号最终连接到哪个 PAD。**

它并不会告诉 Linux：

如何配置这些 PAD。

------

# 4. ARMTmux 的作用

ARMTmux 页面实际上就是：

> **PadMux 寄存器说明书。**

例如：

```
PAD_GPIOD_03

reg_i2s0_4w_tx_mode
```

表示：

PAD_GPIOD_03 可以选择很多功能：

```
GPIO

UART

SPI

LCD

I2S0_TX_SDO
```

其中：

```
reg_i2s0_4w_tx_mode
```

决定：

到底输出哪一种功能。

因此：

ARMTmux 只是：

**告诉驱动应该写哪个 PadMux 寄存器。**

------

# 5. padmux.dtsi 的作用

Linux 并不会直接操作寄存器。

而是在：

```
pcupid-ssm001c-s01a-voip-padmux.dtsi
```

里面写：

例如：

```
<PAD_OUTP_CH2
 PINMUX_FOR_I2S0_RX_MODE_3
 MDRV_PUSE_I2S0_RX_BCK>
```

这句话可以翻译成：

> 将 PAD_OUTP_CH2 配置成 I2S0_RX_BCK。

例如：

```
<PAD_GPIOD_03
 PINMUX_FOR_I2S0_4W_TX_MODE_4
 MDRV_PUSE_I2S0_TX_SDO>
```

表示：

> 将 PAD_GPIOD_03 配置成 I2S0_TX_SDO。

可以发现：

padmux.dtsi 实际上就是：

**ARMTmux 的 Linux 描述。**

------

# 6. 为什么看起来都是 4W？

这是最容易误解的地方。

例如：

```
PINMUX_FOR_I2S0_4W_TX_MODE_4
```

很多人会认为：

既然叫：

```
4W
```

是不是：

系统就是：

```
4 Wire
```

实际上并不是。

需要区分：

## PadMux

PadMux 回答的是：

```
TX_BCK

↓

哪个 PAD
```

例如：

```
TX_BCK

↓

PAD_PWM_ADC00
```

PadMux 并不关心：

RX 有没有独立的 BCLK。

------

## I2S Controller

I2S Controller 回答的是：

```
TX_BCLK

是否

与

RX_BCLK

共用。
```

例如：

4 Wire：

```
TX_BCLK
      │
      └────────► RX
```

6 Wire：

```
TX_BCLK

RX_BCLK
```

这是：

I2S Controller 内部逻辑。

不是 PadMux。

因此：

```
PINMUX_FOR_I2S0_4W_TX_MODE_4
```

只是：

一个历史遗留的宏名字。

不能代表：

I2S 工作模式。

------

# 7. padmux 是否已经支持 6-Wire？

目前分析认为：

答案是：

**支持。**

原因如下。

------

## 证据一

padmux 中：

RX 已经拥有：

```
PAD_OUTP_CH2

↓

I2S0_RX_BCK
```

以及：

```
PAD_OUTN_CH2

↓

I2S0_RX_WCK
```

说明：

RX 已经拥有独立的：BCLK 和 WCK，

而不是与 TX 共用。

------

## 证据二

USBL 管脚定义：

```
TX_BCLK

↓

PAD_GPIOD_04
```

而：

```
RX_BCLK

↓

PAD_OUTP_CH2
```

说明：

PCB 上：

TX_BCLK

与

RX_BCLK

本身就是：

两条独立网络。

说明：

硬件已经按照：

6 Wire

布线。

------

## 证据三

pcupid.dtsi 中明确声明：

```
i2s-trx-shared-padmux = <0>;

i2s-tx-padmux = <6>;

i2s-rx-padmux = <7>;
```

并且：

```
i2s-tx0-tdm-wiremode = <2>;

i2s-rx0-tdm-wiremode = <2>;
```

厂家注释已经说明：

```
wiremode = 2
```

表示：

```
6 Wire
```

因此：

平台本身设计目标就是：

6 Wire。

------

# 8. 为什么 PadMux 与 WireMode 分开？

因为：

PadMux 负责：

```
PAD

↓

什么功能
```

而：

WireMode 决定：

```
I2S Controller

↓

如何驱动这些 PAD。
```

二者职责完全不同。

例如：

PadMux：

```
PAD_OUTP_CH2

↓

RX_BCLK
```

但是：

真正输出：

RX_BCLK

还是：

TX_BCLK

共享，

则由：

```
wiremode
```

决定。

------

# 9. 当前分析结论

目前结合：

USBL 管脚定义

ARMTmux

padmux.dtsi

pcupid.dtsi

可以得到如下结论。

| 模块             | 是否已经支持6-Wire | 说明                                                   |
| ---------------- | ------------------ | ------------------------------------------------------ |
| PCB（USBL 文档） | ✔                  | TX_BCLK 与 RX_BCLK 已独立布线。                        |
| ARMTmux          | ✔                  | 提供了 TX 与 RX 的独立 PadMux 功能。                   |
| padmux.dtsi      | ✔                  | 已分别配置 TX PAD 与 RX PAD，并未共用 PAD。            |
| pcupid.dtsi      | ✔                  | 已配置 6-Wire 模式（wiremode=2）。                     |
| I2S Controller   | 待验证             | 需要确认 sstar_bach 是否真正配置了 6-Wire 控制寄存器。 |

------

# 10. 后续建议

目前 PadMux 部分基本已经分析完成。

下一步建议继续分析：

```
pcupid.dtsi
      │
      ▼
sstar_bach
      │
      ▼
I2S Register
```

重点关注：

- i2s-tx-padmux=<6> 如何被解析；
- i2s-rx-padmux=<7> 如何被解析；
- i2s-tx0-tdm-wiremode=<2> 最终写入哪个寄存器；
- 该寄存器是否真正开启了 TX_BCLK/TX_WCK 与 RX_BCLK/RX_WCK 的独立工作。

完成这一部分后，就能够完整建立：

```
Device Tree
        │
        ▼
PadMux
        │
        ▼
sstar_bach
        │
        ▼
I2S Register
        │
        ▼
6-Wire 真正生效
```

整个配置链也将完全闭环。