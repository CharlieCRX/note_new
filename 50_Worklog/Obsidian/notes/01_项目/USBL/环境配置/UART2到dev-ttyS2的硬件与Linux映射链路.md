# 问题来源 

HCM360B 通过 RS485 连接开发板时， 需要确认为什么使用`/dev/ttyS2`进行通讯。

# 背景知识1：如何确定`ttyS2` 对应了RS485` 

`UART2 连接 RS485` 这件事是 **硬件连线决定的**；

DTS 决定的是：CPU 的哪两个 PAD 被切换成 `UART2_RX / UART2_TX`，以及这个 UART 控制器在 Linux 里变成 `/dev/ttyS2`。

## 1.IO 配置表说明：这些 PAD 可以当 UART2 用

在 `SSD2351_IO配置表格.xlsx` 中：

```c
PAD_OUTP_RX0_CH[2]  -> FUART2_RX
PAD_OUTN_RX0_CH[2]  -> FUART2_TX
```

这张表类似“多功能螺丝刀插槽配置：
一个 PAD 就像一个多功能螺丝刀通用插槽，可以用于接入`MIPI`、`TTL`、`I2S`、`UART` 等不同的批头，但同一时间只能安装一种披头进行工作。

## 2.HW Checklist 进一步确认这些 PAD 的复用能力

在 `SSD2351X_HW Checklist` 的 `ARMTmux / GPIO List` 里，能够看到：

```tex
PAD_OUTP_RX0_CH[2]  可复用为 FUART2_RX
PAD_OUTN_RX0_CH[2]  可复用为 FUART2_TX
```

当 4-pin 模式字段 `reg[0x103cDE][6:5]` = 3，或者对应的 2-pin 模式字段 `reg[0x103cE0][3:2]` = 3 时，该 PAD 位于 FUART2_RX 对应的复用方案中。

两组寄存器分别属于 FUART2 的 4-pin 和 2-pin mux 配置，并非完全相同的配置入口。

## 3.DTS 负责把这两个 PAD 真正切到 UART2

当前 DTS padmux 配置在：

`pcupid-ssm001c-s01a-voip-padmux.dtsi` (line 382)

```
<PAD_OUTP_RX0_CH2 PINMUX_FOR_FUART2_2W_MODE_3 MDRV_PUSE_UART2_RX>,
<PAD_OUTN_RX0_CH2 PINMUX_FOR_FUART2_2W_MODE_3 MDRV_PUSE_UART2_TX>,
```

这一步像“把多功能插座拨到 UART2 档位”。
如果不配这个，硬件上即使连到 RS485 芯片，CPU 管脚也可能还在干别的事。

## 4.USBL 管脚定义表说明板级用途是“UART2 转 RS485”

在 `USBL水面设备管脚定义及器件地址说明.xlsx` 的 `CPU SSD2351D` sheet 中：

```
PAD_OUTP_RX0_CH[2]  CPU_UART2_RX_3V3  IN   UART2接口转RS485
PAD_OUTN_RX0_CH[2]  CPU_UART2_TX_3V3  OUT
```

这就是板级设计证据。它告诉我们：硬件工程师把 CPU 的 `UART2 TX/RX` 这两根线接到了板上的 RS485 转换电路。

## 5.DTS + 驱动决定它在 Linux 里叫 `/dev/ttyS2`

```
fuart2 -> serial2 -> line 2
dev_name = ttyS
最终就是 /dev/ttyS2
```

## 类比

`UART2 连接 RS485` 不是单独由 DTS 决定的，而是三层共同完成：

```tex
芯片能力表决定：这些 PAD 有没有资格做 UART2
DTS 决定：当前系统启动后，这些 PAD 真的被切成 UART2
原理图/板级管脚表决定：UART2 的 TX/RX 实际接到了 RS485 芯片
驱动 + alias 决定：这个 UART2 在 Linux 中叫 /dev/ttyS2
```

- `PAD`： 一个通用螺丝刀插口；
- `管脚复用`：决定了这个通用插口可以接哪些批头；
- `DTS`：通用接口当前配置的批头是什么（一字型）
- `硬件连线`：这个一字型螺丝刀头，要连接哪个一字螺丝；
- `/dev/ttyS2` ：就是这个一字螺丝批头的名称；

# 完整链路