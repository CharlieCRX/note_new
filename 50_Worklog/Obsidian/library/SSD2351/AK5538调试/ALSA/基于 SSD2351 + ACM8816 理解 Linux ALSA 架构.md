# 一、学习目标

本学习并非以掌握 ACM8816 Codec 为目标，而是借助 SSD2351 开发板上的 ACM8816，深入理解 Linux ALSA/ASoC 音频子系统的整体架构。

学习完成后，应能够回答以下问题：

- ALSA 与 ASoC 分别负责什么？
- Codec Driver、CPU DAI Driver、PCM Driver、Machine Driver 各自负责什么？
- `aplay`、`arecord` 为什么能够看到设备？
- PCM Device 是什么时候创建的？
- `capture`、`playback` 声明为什么能够影响 `arecord -l` 的输出？
- `arecord` 从用户空间到 I2S RX FIFO 的完整调用链是什么？
- 一帧 PCM 数据是如何流入内存的？

最终目标不是掌握 ACM8816，而是掌握整个 Linux ASoC 框架。

------

# 二、学习路线

整个学习路线按照真实数据流进行，而不是按照源码目录阅读。

```text
用户程序
   │
   ▼
arecord / aplay
   │
   ▼
ALSA Userspace(libasound)
   │
   ▼
ALSA Core
   │
   ▼
ASoC Framework
   │
   ▼
Machine Driver
   │
 ┌─┴──────────────┐
 │                │
CPU DAI       Codec DAI
 │                │
 │             Codec Driver
 │
PCM Driver(DMA)
 │
 ▼
I2S Controller
 │
 ▼
Codec
```

整个课程就是沿着这条数据流逐步理解。

------

# 三、实践一：Codec DAI 如何影响 ALSA 设备

理解：

> Codec Driver 中的 DAI 描述并不仅仅是描述 Codec，而是决定了 ALSA 最终向用户空间暴露哪些能力。

这是理解 ALSA 的第一步。

具体参考：[[实践一(1)：Codec DAI 如何影响 ALSA 设备]]

------

# 四、实践二：PCM Device 是什么时候创建的？

这是下一步研究的问题。

建议阅读：

Machine Driver

例如：

```text
sstar-asoc-card
```

重点研究：

```c
snd_soc_register_card()
```

研究：

它什么时候：

创建：

Card

DAI Link

PCM Device

理解：

为什么：

```bash
aplay -l
```

能够看到：

device0。

------

# 五、实践三：为什么 arecord 能开始录音？

研究：

```bash
arecord
```

开始执行后：

调用：

```text
open()

↓

hw_params()

↓

prepare()

↓

trigger()
```

对应源码：

```text
soc-pcm.c
```

理解：

什么时候：

真正开始配置硬件。

------

# 六、实践四：DMA 是什么时候启动？

研究：

```text
sstar_pcm.c
```

重点：

理解：

DMA Buffer

DMA Descriptor

DMA Start

Pointer

Interrupt

------

# 七、实践五：CPU DAI 做了什么？

研究：

```text
sstar_bach.c
```

理解：

I2S Controller

如何：

配置：

BCLK

LRCK

Word Length

Master

Slave

FIFO

TX

RX

------

# 八、实践六：Codec Driver 真正负责什么？

重新回到：

```text
acm8816.c
```

研究：

Codec Driver

真正负责：

I2C

寄存器

Mute

Volume

PLL

ADC

DAC

而不是：

DMA。

不是：

PCM。

不是：

I2S Controller。

------

# 九、最终完成的数据流理解

学习完成后，应能够完整画出：

```text
arecord
        │
        ▼
libasound
        │
        ▼
ALSA Core
        │
        ▼
ASoC Framework
        │
        ▼
Machine Driver
        │
 ┌──────┴────────┐
 │               │
 ▼               ▼
CPU DAI      Codec DAI
 │               │
 ▼               ▼
PCM Driver    Codec Driver
 │
 ▼
DMA
 │
 ▼
I2S Controller
 │
 ▼
Codec ADC
 │
 ▼
PCM Samples
 │
 ▼
DMA Buffer
 │
 ▼
arecord 保存 wav
```

------

# 十、整个学习路线建议

我建议整个学习过程坚持以下原则：

| 阶段     | 学习方式                       | 输出成果         |
| -------- | ------------------------------ | ---------------- |
| 第一阶段 | 做一个最小实验（修改一个字段） | 观察系统行为变化 |
| 第二阶段 | 提出"为什么会这样？"           | 建立假设         |
| 第三阶段 | 阅读相关源码验证假设           | 理解模块职责     |
| 第四阶段 | 绘制调用链和数据流图           | 建立整体框架     |
| 第五阶段 | 再做新的实验验证理解           | 巩固知识         |

------

## 我建议增加一个贯穿整个课程的"主线问题"

相比于单独学习各个模块，我更推荐围绕一个固定的问题展开整个学习过程：

> **为什么仅仅注释掉 `acm8816_dai` 中的 `.capture` 成员，`arecord -l` 就消失了？**

这个问题会自然引导你依次理解：

1. Codec DAI 是什么？
2. DAI 是如何注册到 ASoC Framework 的？
3. Machine Driver 如何把 CPU DAI 和 Codec DAI 连接起来？
4. PCM Device 是什么时候创建的？
5. `aplay -l` / `arecord -l` 为什么能够枚举设备？
6. `arecord` 真正开始录音时，又是如何一步步走到 `sstar_pcm`、`sstar_bach`，最终启动 SSD2351 的 I2S RX 和 DMA 的？

**这样，整个 ALSA 学习过程就不再是阅读零散源码，而是围绕一个真实实验逐层揭开整个音频子系统的工作机制。**我认为这也是最适合你当前 SSD2351 音频调试工作的学习方式。