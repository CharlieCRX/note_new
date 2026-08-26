> 此文档已经被[[USBL 管脚定义与 PadMux 中 PAD 名称差异分析]] 证明不存在冲突问题。

前面我们的分析有一个默认假设——**padmux.dtsi 与 USBL 管脚定义是一致的**。但现在发现，它们**实际上并不一致**。

这意味着，我们需要重新审视整个配置链，而不是简单认为 padmux 已经正确配置。

------

# 首先，把两份文档放在一起比较

## USBL 管脚定义（硬件要求）

根据《USBL 水面设备管脚定义》，I2S0 TX 应该使用下面三个 PAD：

| 信号    | 要求 PAD         |
| ------- | ---------------- |
| TX_BCLK | **PAD_GPIOD_04** |
| TX_WCK  | **PAD_GPIOD_05** |
| TX_SDO0 | **PAD_GPIOD_03** |

RX：

| 信号    | PAD          |
| ------- | ------------ |
| RX_BCLK | PAD_OUTP_CH2 |
| RX_WCK  | PAD_OUTN_CH2 |
| RX_SDI  | PAD_OUTP_CH3 |

------

## padmux.dtsi 实际配置

```c
// I2S0
<PAD_OUTP_CH2            PINMUX_FOR_I2S0_RX_MODE_3        MDRV_PUSE_I2S0_RX_BCK>,
<PAD_OUTN_CH2            PINMUX_FOR_I2S0_RX_MODE_3        MDRV_PUSE_I2S0_RX_WCK>,
<PAD_OUTP_CH3            PINMUX_FOR_I2S0_RX_MODE_3        MDRV_PUSE_I2S0_RX_SDI>,
<PAD_PWM_ADC00           PINMUX_FOR_I2S0_4W_TX_MODE_1     MDRV_PUSE_I2S0_TX_BCK>,
<PAD_PWM_ADC01           PINMUX_FOR_I2S0_4W_TX_MODE_1     MDRV_PUSE_I2S0_TX_WCK>, 
<PAD_GPIOD_03            PINMUX_FOR_I2S0_4W_TX_MODE_1     MDRV_PUSE_I2S0_TX_SDO>, 
```

目前看到的是：

| 信号    | padmux 配置       |
| ------- | ----------------- |
| TX_BCLK | **PAD_PWM_ADC00** |
| TX_WCK  | **PAD_PWM_ADC01** |
| TX_SDO0 | **PAD_GPIOD_03**  |
| RX_BCLK | PAD_OUTP_CH2      |
| RX_WCK  | PAD_OUTN_CH2      |
| RX_SDI  | PAD_OUTP_CH3      |

整理一下：

| 信号    | USBL         | padmux            | 是否一致 |
| ------- | ------------ | ----------------- | -------- |
| TX_BCLK | PAD_GPIOD_04 | **PAD_PWM_ADC00** | ❌        |
| TX_WCK  | PAD_GPIOD_05 | **PAD_PWM_ADC01** | ❌        |
| TX_SDO0 | PAD_GPIOD_03 | PAD_GPIOD_03      | ✅        |
| RX_BCLK | PAD_OUTP_CH2 | PAD_OUTP_CH2      | ✅        |
| RX_WCK  | PAD_OUTN_CH2 | PAD_OUTN_CH2      | ✅        |
| RX_SDI  | PAD_OUTP_CH3 | PAD_OUTP_CH3      | ✅        |

所以目前实际上：

> **只有 TX_BCLK、TX_WCK 两个 PAD 不一致。**

------

# 这意味着什么？

这里有三种可能性，而且我认为应该按优先级去排查。

------

## 第一种（我认为概率最高）：padmux.dtsi 与当前 PCB 不匹配

例如：

SigmaStar SDK 里面提供的是：

```text
Reference Board
```

默认配置：

```text
TX_BCLK
↓

PAD_PWM_ADC00
```

但是：

USBL 项目 PCB：

已经改成：

```text
TX_BCLK
↓

PAD_GPIOD_04
```

如果 DTS 没改，

那么：

CPU 会：

```text
BCLK

↓

PWM_ADC00
```

而 PCB：

真正接 Codec 的却是：

```text
GPIOD_04
```

于是：

```text
Codec
```

根本收不到：

BCLK。

------

画出来就是：

正确情况：

```text
CPU
 │
 ▼
PAD_GPIOD_04
 │
 ▼
Codec
```

现在：

```text
CPU
 │
 ▼
PAD_PWM_ADC00

(PCB没有连接)

PAD_GPIOD_04
 │
 ▼
Codec
```

这样：

Codec 永远没有：

BCLK。

------

## 第二种：GPIOD_04 和 PWM_ADC00 是同一个 PinMux Mode

这个概率比较小。

有些 SoC：

一个：

```text
TX_BCLK
```

可以：

```text
Mode1

↓

PAD_GPIOD_04
```

也可以：

```text
Mode4

↓

PAD_PWM_ADC00
```

例如：

```text
TX_BCLK
```

允许：

```text
GPIOD_04
```

或者：

```text
PWM_ADC00
```

这是：

很多 SoC 都支持的。

如果是这种情况，

那么：

USBL 只是：

选择：

另一组 PAD。

这种情况下：

padmux 就必须：

修改。

否则：

输出到了另一组 Pin。

------

## 第三种：USBL 文档写错了

这种可能性：

不能排除。

有时候：

PCB 改版以后：

文档没有更新。

或者：

文档：

沿用了：

早期版本。

因此：

最终：

应该：

以：

```text
PCB 原理图
```

为准。

------

# 有没有办法判断是哪一种？

有。

而且非常简单。

去看：

**ARMTmux。**

例如：

找到：

```text
PAD_GPIOD_04
```

看看：

是不是：

支持：

```text
I2S0_TX_BCK
```

如果：

ARMTmux：

显示：

```text
PAD_GPIOD_04

↓

I2S0_TX_BCK
```

那么：

说明：

确实：

支持：

GPIOD_04。

再去看：

```text
PAD_PWM_ADC00
```

如果：

也支持：

```text
I2S0_TX_BCK
```

说明：

SoC：

支持：

两套：

TX Pin。

这时候：

padmux 就必须：

选择：

与你 PCB 一致的那一套。

------

# 为什么只有 BCLK/WCK 不一致？

这反而说明：

不是随机错误。

因为：

SDO：

已经：

一致：

```text
PAD_GPIOD_03
```

RX：

全部：

一致。

只有：

```text
TX_BCLK

TX_WCK
```

不同。

这说明：

**SDK 很可能默认采用了另一组 TX Clock PAD，而 USBL 板卡为了布线方便，把 TX Clock 挪到了 GPIOD_04/GPIOD_05。**

这种情况在 SoC 项目里非常常见。

------

# 对你的项目意味着什么？

我建议**暂停继续分析 sstar_bach**，先把 PadMux 是否匹配 PCB 这个问题彻底确认。

因为：

如果：

PadMux：

已经：

错了。

那么：

后面：

```text
wiremode

controller

DMA

ASoC
```

全部：

都是：

建立在：

错误 PAD 上。

------

## 我建议下一步验证下面两个问题

### ① 查看 ARMTmux

重点查四个 PAD：

- PAD_GPIOD_04
- PAD_GPIOD_05
- PAD_PWM_ADC00
- PAD_PWM_ADC01

确认：

它们分别支持哪些 I2S0 功能。

特别确认：

**GPIOD_04/05 是否支持 I2S0_TX_BCK/WCK，以及 PWM_ADC00/01 是否也是 I2S0_TX_BCK/WCK 的另一套复用。**

------

### ② 查看 padmux.dtsi 是否还有另一组 I2S0 配置

很多 SigmaStar 平台会存在：

```dts
i2s0_tx_mode_1
```

和：

```dts
i2s0_tx_mode_4
```

甚至：

```dts
i2s0_tx_mode_6
```

只是：

当前：

Board：

引用了：

其中一个。

如果：

存在：

另一组：

```text
PAD_GPIOD_04

PAD_GPIOD_05
```

那么：

只需要：

修改：

Board：

引用即可。

------

**我的判断（目前置信度约 80%）是：你已经发现了一个真实的配置不一致问题，而不是理解上的误区。最大的可能是 USBL PCB 已经采用了 GPIOD_04/GPIOD_05 作为 TX 时钟引脚，但当前 `pcupid-ssm001c-s01a-voip-padmux.dtsi` 仍然沿用了 SigmaStar SDK 默认开发板使用的 PWM_ADC00/PWM_ADC01 配置。**

**建议下一步不要猜测，而是把 ARMTmux 中这四个 PAD（GPIOD_04、GPIOD_05、PWM_ADC00、PWM_ADC01）对应的完整配置页发出来，我们可以直接判断它们是不是两套等效的 I2S0 TX 引脚组，以及当前 DTS 是否确实配置错了。**