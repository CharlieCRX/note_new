今天任务就是，将需求梳理清晰：

- 项目要求我做什么？

# 亟待补充的任务细节

任务描述为:

> ## USBL  `i2s0` 的 6线收发验证
>
> 任务描述：
>
> 现在修改的 sdk 已支持 acm8816 通过 i2s tx发送，第一步验证 `i2s0` 6线模式下 rx和tx收发功能。 
>
> fpga把ak5538接入模拟es7243做rx验证
>
> 1、通过fpga模拟的rx数据进行i2s0的rx调试。
>
> 2、将neu的es7243e的驱动合入、验证acm8816和es7243e可以同时使用

除此任务描述外，还有文档《USBL水面设备管脚定义及器件地址说明》来补充一些背景。

# 原理框图

根据《USBL水面设备管脚定义及器件地址说明》中原理框图的描述，AK5538和ACM8816功能为：

```
		换能器
   ↑              ↓
发射声波        接收回波

ACM8816        AK5538
   ↑              ↓

	SSD2351 (主控/DSP)
```

其中 ACM8816 在系统中的职责为：

> 将SSD2351生成的数字发射波形通过I2S接收，转换并放大成模拟信号，经变压器升压后驱动换能器发射声波。

# 验证 ACM8816 的 I2S TX

任务中，已经描述了当前的 SDK 已经支持了SSD2351 通过 I2S TX 向 ACM8816 发送数据。

那么要理解：

- SDK
- [[#I2C 和 I2S 是完全不同的两条总线]]
- [[#TX 和 RX 到底是谁发谁收？]]

这几个前置知识。

# I2C 和 I2S 是完全不同的两条总线

## I2C

I2C 用来：

> **配置芯片。**

例如：

```
ACM8816

寄存器0x10

← 写入

0x18
```

意思就是：

```
音量 = 24
```

I2C 上传输的是：

- 寄存器地址
- 寄存器数据

它不是音频。

## I2S


##  PCM 数据

**PCM**（Pulse Code Modulation，脉冲编码调制）是一种**数字音频的编码格式**，本质上是**模拟信号经过“采样”和“量化”后得到的数字序列**。

简单说，PCM数据就是**声音在时间轴上的“快照数值”列表**：

- **采样（时间轴）**：把连续的模拟声波，按固定时间间隔（如每秒44100次）切分。
- **量化（幅度轴）**：把每个切分点的电压高度，用一个整数（如16bit的0到65535，或带符号的-32768~32767）来表示。

## I2S 和 PCM 之间的关系

**I2S 是“运输货车”，PCM 是“货物”。**

- **PCM** 决定了“货物长什么样”（比如是16bit还是24bit的整数，是有符号还是无符号）。
- **I2S** 决定了“货物怎么装车、怎么运”（比如：在SCK的哪个时钟沿抓取数据，WS为高电平时是左声道还是右声道，数据是MSB（高位）先发还是LSB（低位）先发）。

再看框图：

```
SSD2351
   │
 ┌─┴─────────┐
 │           │
I2C        I2S
 │           │
 └────ACM8816┘
```

这是几乎所有 Audio Codec 的标准连接方式。

因为：

一条总线负责：

> 配置。

另一条总线负责：

> 传输数据。

# TX 和 RX 到底是谁发谁收？

TX 到底是谁的 TX？

答案永远是：**相对于 SSD2351 而言。**

因为：SSD2351 是主控。

所以：

```
SSD2351 TX

=

ACM8816 RX
```

反过来也是，如果 ACM8816 向 SSD2351 发送数据，则

```
SSD2351 RX

=

AK5538 TX
```

# 补充背景知识后的需求理解

> SDK 支持 ACM8816 I2S TX

意思一般是：

> SDK 支持 SSD2351 的 I2S TX，把音频发送给 ACM8816。
>
> 经过烧录以后，Linux 音频子系统具备了"SSD2351 I2S 接口 + ACM8816 Codec"这一套完整的播放能力。

即：

- SSD2351 - I2S TX - ACM8816

ACM8816 负责接收 PCM ，将数字信号变为模拟信号。

# BSP系统

整个 SDK 的组成，它实际上不是一个单独的软件，而是一个完整的 BSP（Board Support Package），包含 Boot、Kernel、Project、SDK 四部分。

```
boot        --> U-Boot
kernel      --> Linux Kernel
project     --> 打包 Image、Rootfs、ko
sdk         --> 用户态 Demo / API / 应用
```

# 替换的 kernel 文件

按照手册全局编译得到初始工程，然后将提供的`kernel`目录中的文件：

```bash
kernel
|-- arch
|   `-- arm64
|       |-- boot
|       |   `-- dts
|       |       `-- sstar
|       |           |-- pcupid-ssm001c-s01a-voip-padmux.dtsi
|       |           |-- pcupid-ssm001c-s01a-voip.dts
|       |           `-- pcupid.dtsi
|       `-- configs
|           `-- pcupid_ssm001c_s01a_spinand_voip_defconfig
`-- sound
    `-- soc
        |-- codecs
        |   |-- Kconfig
        |   |-- Makefile
        |   |-- acm8816.c
        |   `-- acm8816.h
        `-- sstar
            `-- pcupid
                |-- sstar_asoc_card.c
                |-- sstar_bach.c
                `-- sstar_pcm.c
```

 一一复制到对应刚刚得到的初始工程对应的目录中。

> 为什么需要 acm8816.c、sstar_pcm.c、sstar_bach.c、sstar_asoc_card.c 这么多文件？
>
> 每个文件到底负责什么？
>
> PCM 数据又是怎么流过去的？

结合 `acm8816_launch`应用层软件，综合分析：[[acm8816Launch 发生了什么]]

# 当前任务清单

经过梳理后，可以总结下真正的需求是：

> SSD2351 I2S0 Controller 是否已经工作在 Full Duplex（6-wire）模式。

建议按下面的顺序检查：

1. **Device Tree**

   - `i2s0` 节点是否有 4-wire / 6-wire 配置。
   - 是否配置了 `playback` 和 `capture`。

2. **CPU DAI Driver（最关键）**

   - 搜索关键字：

     ```
     6wire
     4wire
     wire
     tx
     rx
     bach
     i2s
     ```

   - 找到 I2S 控制器寄存器初始化，确认 6 线模式是如何配置的。

3. **Machine Driver (`sstar_asoc_card`)**

   - 查看 `snd_soc_dai_link` 是否同时支持 Playback 和 Capture。
   - 是否有 `capture_only`、`playback_only` 等限制。

4. **Platform Driver (`sstar_pcm`)**

   - 确认是否同时注册了 Playback DMA 和 Capture DMA。

5. **Codec Driver (`acm8816`)**

   - 查看 `snd_soc_dai_driver` 中是否声明了：

     ```
     .playback = { ... },
     .capture  = { ... },
     ```

   - 如果只有 `.playback`，那么 ACM8816 本身就是播放设备，任务中的 RX 就更可能是在验证 **CPU I2S0 控制器的 RX 能力**，而不是 ACM8816 返回音频数据。
