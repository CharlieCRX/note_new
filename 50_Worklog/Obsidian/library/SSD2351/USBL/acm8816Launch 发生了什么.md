------

# 从 `acm8816_launch.c` 到 ACM8816，到底发生了什么？

假设现在执行：

```bash
./acm8816Launch -f 40000 -R 96000 -b 16
```

整个系统实际上会经历下面这一条调用链：

```text
                 用户程序 (acm8816Launch)
                          │
                          ▼
               ALSA Userspace (libasound)
                          │
                     syscall
                          │
                          ▼
                     ALSA Core
                          │
          ┌───────────────┼───────────────┐
          │               │               │
          ▼               ▼               ▼
   ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
   │  Machine    │ │  Platform   │ │   Codec     │
   │  Driver     │ │  Driver     │ │   Driver    │
   │(sstar_asoc_ │ │ (sstar_pcm) │ │ (acm8816)   │
   │  card)      │ └──────┬──────┘ └──────┬──────┘
   └──────┬──────┘        │ 注册/拥有      │ 注册/拥有
          │               ▼               ▼
          │        ┌─────────────┐ ┌─────────────┐
          │        │  CPU DAI    │ │  Codec DAI  │
          │        │  Driver     │ │  Driver     │
          │        │(sstar_bach) │ │ (acm8816)   │
          │        └──────┬──────┘ └──────┬──────┘
          │               │               │
          └───────────────┴───────────────┘
                       ▲
                       │  (Machine 驱动通过 dai_link 进行绑定)
                       │
                  DAI Link (契约)
                       │
                       ▼ (硬件实例化)
              ┌─────────────────────┐
              │ SSD2351 I2S 控制器 │ (CPU 端的物理硬件)
              └──────────┬──────────┘
                         │
        BCLK / LRCK / SDOUT / SDIN (物理铜线)
                         │
                         ▼
              ┌─────────────────────┐
              │  ACM8816 Codec 芯片 │ (Codec 端的物理硬件)
              └──────────┬──────────┘
                         │
                         ▼
                    DAC → OUTP/OUTN
```

这是一个以 CPU DAI Driver（`sstar_bach.c`）为核心、Machine Driver（`sstar_asoc_card.c`）负责连接、Codec Driver（`acm8816.c`）负责参数匹配的 ALSA ASoC 全双工验证任务。

> 注意：
>
> - **明确了“注册者”身份**：图中 `Platform Driver` 下方缩进写出了 `CPU DAI Driver`，这告诉读者：**DAI 不是凭空出现的，是 Platform 驱动在 `probe` 时调用 `snd_soc_register_dai()` 注册进内核的**。
> - **把 Machine Driver 提升为“总导演”**：Machine Driver 位于最顶层左侧，通过 `dai_link` 结构体“遥指”下方的两个 DAI。这精准对应了 `snd_soc_dai_link` 结构体中的 `.cpus`、`.codecs` 和 `.platforms` 成员变量。
> - **完美区分了“软件对象”和“物理硬件”**：“DAI Link”下面首次出现了“SSD2351 I2S 控制器”和“ACM8816 Codec 芯片”——这正好对应了你文中说的：**DAI 是代理人，而 I2S 控制器和 Codec 芯片才是被代理人（物理实体）**。

------

# 第一层：acm8816_launch.c（用户程序）

这是你编写的用户态程序，也是整个播放流程的起点。

它的职责非常简单：

> **准备 PCM 数据，并把 PCM 数据交给 ALSA。**

例如，你的程序首先调用：

```c
snd_pcm_open(...)
```

这一句的作用并不是打开 I2S，而是告诉 Linux：

> 我要打开一个播放设备（Playback Device）。

例如：

```c
snd_pcm_open(
    &pcm_handle,
    "hw:0,0",
    SND_PCM_STREAM_PLAYBACK,
    0);
```

其中：

- `hw:0,0` 表示第 0 张声卡（Audio Card）
- 第 0 个 PCM Device

这个 Audio Card 就是后面 `sstar_asoc_card.c` 注册出来的那块声卡。

随后，你的程序继续配置播放参数，例如：

```c
snd_pcm_hw_params_set_format()
```

指定数据格式：

```text
S16_LE（16 位 PCM）
```

然后：

```c
snd_pcm_hw_params_set_rate()
```

指定采样率：

```text
96000 Hz
```

再调用：

```c
snd_pcm_hw_params_set_channels()
```

指定声道数，例如单声道。

这些 API 都是在告诉 ALSA：

> **我要播放一段 96kHz、16bit、单声道的 PCM 数据。**

真正的数据发送发生在：

```c
snd_pcm_writei(...)
```

例如：

```c
snd_pcm_writei(
    pcm_handle,
    current_data,
    frames_to_play);
```

这里把一段 PCM Buffer 交给 ALSA。

注意，这里**还没有开始发送 I2S**，只是把数据交给了内核。

------

# 第二层：ALSA Core

ALSA Core 可以理解成整个 Linux 音频系统的调度中心。

它收到 `snd_pcm_writei()` 后，会检查很多内容，例如：

- 这块 Audio Card 是否存在？
- 当前是否支持 Playback？
- 96kHz 是否支持？
- 16bit 是否支持？
- 当前 PCM Device 是否已经打开？

具体 ALSA 的作用，可以参考：[[ALSA-高级Linux声音架构]]

确认都没有问题以后，它才会调用下面几个 Driver。

------

# 第三层：Machine Driver（sstar_asoc_card.c）

很多人第一次学习 ALSA 时都会疑惑：

> Machine Driver 到底干什么？

实际上，它最大的职责只有一个：

> **描述整块声卡的拓扑结构。**

例如，它会告诉 ALSA：

```text
SSD2351 I2S0
        │
        ▼
    ACM8816 Codec
```

或者：

```text
SSD2351 I2S1
        │
        ▼
     AK5538 ADC
```

也就是说，它负责建立：

```text
CPU DAI  <------->  Codec DAI
```

之间的连接关系。
- CPU DAI 是 CPU 端 I2S 控制器的软件化身
- Codec DAI 是音频芯片数字接口的软件化身

如果没有 Machine Driver，ALSA 根本不知道 SSD2351 的 I2S 到底连接的是 ACM8816 还是其它 Codec。

因此，它几乎不参与 PCM 数据传输，而是负责描述"谁和谁连接在一起"。

其结构：

```bash
sstar_asoc_card.c
        │
        ├── 注册 Audio Card
        ├── 注册 DAI Link
        ├── 绑定 CPU DAI
        ├── 绑定 Codec DAI
        └── 告诉 ALSA 整块板子的连接关系
```




------

# 第四层：Codec Driver（acm8816.c）

这是最容易产生误解的一层。

很多人会认为：

```text
snd_pcm_writei()
        │
        ▼
    acm8816.c
```

实际上，**PCM 数据几乎不会经过 `acm8816.c`。**

`acm8816.c` 的职责主要是：

> **通过 I2C 配置 ACM8816。**

例如：

- 设置采样率（96kHz）
- 设置位宽（16bit）
- 设置 I2S 工作模式
- 打开 DAC
- 设置音量
- 解除静音

这些操作都是通过 I2C 写 ACM8816 的寄存器完成的。

因此，在真正播放之前，Codec Driver 会把 ACM8816 配置到正确的工作状态。

配置完成以后，它基本就不再参与 PCM 数据的搬运。

所以可以把它理解成：

> **配置 ACM8816，而不是传输 PCM。**

------

# 第五层：PCM Driver（sstar_pcm.c）

真正负责 PCM 数据搬运的是 `sstar_pcm.c`。

它收到 ALSA 传下来的 PCM Buffer 后，会完成下面几件事情：

1. 分配 DMA Buffer。
2. 配置 DMA 描述符。
3. 设置 DMA 的源地址和目标地址。
4. 启动 DMA。

DMA 的搬运方向可以理解成：

```text
DDR Memory
      │
      ▼
DMA Controller
      │
      ▼
I2S TX FIFO
```

这里有一个非常重要的概念：

CPU **不会一字节一字节地发送 PCM 数据**。

CPU 的工作只是：

> **配置 DMA。**

真正的数据搬运全部由 DMA 控制器自动完成。

因此，CPU 的负担非常小。

------

# 第六层：CPU DAI Driver（sstar_bach.c）

如果说 `sstar_pcm.c` 负责"搬数据"，那么 `sstar_bach.c` 就负责"控制 I2S 硬件"。

它操作的是 SSD2351 内部的 Audio Controller。

例如：

- 配置 BCLK
- 配置 LRCK
- 设置采样率
- 设置数据位宽
- 打开 I2S TX
- 配置 FIFO
- 配置 DMA Request

这些最终都会写入 SSD2351 的 I2S 控制器寄存器。

因此，真正控制 I2S TX 引脚的是 `sstar_bach.c`。

DAI 具体逻辑可以参考：[[CPU DAI vs Codec DAI]]

------

# 第七层：SSD2351 I2S Controller

当 DMA 已经把 PCM 数据搬运到 I2S FIFO 后，SSD2351 的 I2S 控制器开始工作。

它会按照 I2S 协议，把 FIFO 中的数据一位一位地发送出去。

例如：

- BCLK：每一位数据的时钟
- LRCK：左右声道同步时钟
- SDOUT：PCM 数据

因此，在示波器上看到的 I2S 波形，就是这一层产生的。

------

# 第八层：ACM8816

ACM8816 一直工作在 I2S 接收模式。

它收到来自 SSD2351 的 PCM 数据以后，就开始进行 DAC 转换：

```text
PCM Data
     │
     ▼
DAC
     │
     ▼
模拟电压
     │
     ▼
OUTP / OUTN
```

随后经过功放、变压器和匹配网络，最终驱动换能器发射水声信号。

------

# 为什么 `acm8816_launch.c` 完全没有 I2C 操作？

这是一个非常关键的现象。

如果你重新阅读整个 `acm8816_launch.c`，你会发现里面几乎全部都是：

```c
snd_pcm_open()

snd_pcm_hw_params()

snd_pcm_prepare()

snd_pcm_writei()

snd_pcm_drain()
```

**没有任何一行代码直接访问 I2C。**

原因就在于：

> **I2C 的所有细节已经被 `acm8816.c` 内核驱动封装起来了。**

用户程序只需要告诉 ALSA：

> 我要播放一段 96kHz、16bit 的 PCM。

至于：

- ACM8816 是否需要解除静音？
- DAC 是否需要 Enable？
- Word Length 应该设置多少？
- I2S 是 Master 还是 Slave？

这些全部由 `acm8816.c` 自动完成。

因此，对于用户程序来说，它根本不需要关心 ACM8816 的寄存器细节。

------

## 我建议我们下一步怎么分析

我认为，我们已经把**整个调用链**建立起来了，但这还只是"宏观架构"。

下一步最有价值的事情，不是继续讲概念，而是**沿着一次 `snd_pcm_writei()` 的实际调用栈分析源码**。

例如：

1. `snd_pcm_open()` 最终是如何找到 `sstar_asoc_card.c` 注册的 Audio Card？
2. `snd_pcm_hw_params()` 为什么会同时进入 `acm8816.c` 和 `sstar_bach.c`？
3. `snd_pcm_writei()` 为什么最终会进入 `sstar_pcm.c`？
4. `sstar_pcm.c` 又是如何调用 DMA，把数据送进 SSD2351 的 I2S FIFO？

这样你以后阅读这几个驱动文件时，就不会觉得它们是彼此独立的，而是能够清楚地看到**一条完整的数据流**。这也是后续分析 ACM8816 驱动最自然、最容易理解的方式。