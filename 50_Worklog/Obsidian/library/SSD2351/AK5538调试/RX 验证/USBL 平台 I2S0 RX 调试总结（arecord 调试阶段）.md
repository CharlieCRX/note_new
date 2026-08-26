# USBL 平台 I2S0 RX 调试总结（arecord 调试阶段）

## 一、调试背景

当前目标为验证 **SSD2351 I2S0 RX** 是否能够正常接收 **AK5538 ADC** 输出的数字音频数据。

目前系统的数据流如下：

```text
              FPGA
        ┌──────────────┐
        │              │
        │ 输出 MCLK    │
        │ 输出 BCLK    │
        │ 输出 LRCK    │
        └──────┬───────┘
               │
               ▼
           AK5538（ADC）
               │
               │ SDTO
               ▼
      FPGA（0x23=0）或 SSD2351（0x23=1）
```

其中 FPGA 的 **0x23 寄存器**用于控制 AK5538 的 SDTO 输出方向：

- **0x23 = 0**：SDTO 输出到 FPGA；
- **0x23 = 1**：SDTO 输出到 SSD2351 I2S0 RX（CPU）。

本次调试选择：

```
0x23 = 1
```

即验证 CPU 的 I2S0 RX 接收能力。

------

# 二、为什么使用 arecord

`arecord` 是 Linux ALSA 提供的录音工具，其作用不是配置 AK5538，而是验证 Linux 音频采集链路是否正常。

调用路径如下：

```text
arecord
    │
    ▼
ALSA PCM
    │
    ▼
ASoC
    │
    ▼
CPU I2S RX
    │
    ▼
AI DMA
```

如果 `arecord` 能够正常生成 wav 文件，则说明：

- CPU I2S RX 工作正常；
- AI DMA 工作正常；
- PCM Driver 工作正常；
- ALSA 用户空间正常。

因此，`arecord` 是验证 **CPU 接收链路** 的工具，而不是验证 AK5538 本身。

------

# 三、调试过程

## 1）首次录音

执行：

```bash
arecord -D hw:0,0 \
-r 128000 \
-c 2 \
-f S32_LE \
-d 5 \
test.wav
```

返回：

```
Channels count non available
```

分析：

通过：

```bash
arecord -D hw:0,0 --dump-hw-params
```

得到：

```
CHANNELS: 1
```

说明：

当前 PCM 驱动仅支持：

```
Mono（单声道）
```

因此修改为：

```bash
-c 1
```

继续测试。

------

## 2）第二次录音

执行：

```bash
arecord \
-D hw:0,0 \
-r 128000 \
-c 1 \
-f S32_LE \
-d 5 \
test.wav
```

返回：

```
Warning:
requested = 128000
got = 192000

Segmentation fault
```

说明：

ALSA 参数协商阶段已经通过。

但是驱动最终将采样率配置为了：

```
192000 Hz
```

随后程序发生：

```
Segmentation fault
```

------

## 3）第三次录音

继续尝试：

```bash
arecord \
-D hw:0,0 \
-r 192000 \
-c 1 \
-f S32_LE \
-d 5 \
test.wav
```

仍然：

```
Segmentation fault
```

因此可以排除：

- 128k 参数问题；
- 采样率协商问题。

------

# 四、Kernel Oops 分析

查看 dmesg 后得到：

```
Unable to handle kernel paging request
```

以及：

```
pc : __memset
lr : snd_pcm_hw_params
```

Call Trace：

```
arecord
    ↓
snd_pcm_ioctl
    ↓
snd_pcm_hw_params
    ↓
__memset
    ↓
Kernel Oops
```

说明：

Kernel 在执行：

```
snd_pcm_hw_params()
```

过程中，对一块非法地址进行了：

```
memset()
```

导致：

```
Translation Fault
```

因此：

当前崩溃发生于：

```
ALSA Core
```

而不是：

```
mhal_audio_ai_start()

或者

DMA Interrupt
```

说明：

目前甚至尚未进入真正的数据采集阶段。

------

# 五、日志中发现的问题

日志中还有如下信息：

```
Enter acm8816_pcm_hw_params
```

随后：

```
err: i2c-1 no ack
```

说明：

当前 Capture 链路仍然进入了：

```
ACM8816 Codec Driver
```

而当前板上实际使用的是：

```
AK5538
```

因此：

ACM8816 不存在，

I2C 自然：

```
NO ACK
```

目前推测：

Machine Driver 中 Capture Codec 仍然绑定到了 ACM8816。

需要进一步确认：

```
snd_soc_dai_link
```

配置。

------

# 六、sstar_pcm.c 分析

分析 `sstar_pcm.c` 后确认：

在：

```
sstar_pcm_hw_params()
```

中：

```c
runtime->dma_area
runtime->dma_addr
runtime->dma_bytes
```

全部来自：

```
ss_pcm_stream->buf
```

而 DMA Buffer 来源于：

```c
alloc_dmem()

↓

msys_request_dmem()
```

即：

```
buf->area
```

随后：

```c
runtime->dma_area
=
buf->area
```

结合 Kernel Oops：

```
__memset()

↓

runtime->dma_area

↓

Translation Fault
```

目前怀疑：

DMA Buffer 指针异常。

需要进一步确认：

```
msys_request_dmem()
```

返回的：

```
kvirt
```

是否为真正可供 CPU 访问的 Kernel Virtual Address。

------

# 七、当前已排除的问题

目前已经确认：

✅ ALSA Card 注册成功。

✅ PCM Device 创建成功。

✅ `hw:0,0` 可以正常打开。

✅ PCM 支持：

```
S16_LE
S32_LE
```

✅ PCM 支持：

```
48000 ~ 192000
```

✅ PCM 支持：

```
Channel = 1
```

因此：

问题已经不是：

- ALSA 参数错误；
- arecord 参数错误；
- 通道数错误；
- 采样率错误。

------

# 八、当前仍需确认的问题

目前仍存在两个独立的问题。

## 问题一：Capture Codec 配置

日志显示：

```
Capture

↓

ACM8816 Driver
```

但当前实际使用：

```
AK5538
```

需要确认：

Machine Driver 是否仍然绑定：

```
codec = ACM8816
```

如果是：

需要修改为：

- AK5538 Codec Driver（如果实现）；
- 或 Dummy Codec（AK5538 已由外部 I2C 初始化）。

------

## 问题二：PCM DMA Buffer

Kernel Oops 表明：

```
snd_pcm_hw_params()

↓

runtime->dma_area

↓

memset()

↓

Translation Fault
```

因此重点需要检查：

```
alloc_dmem()

↓

msys_request_dmem()
```

以及：

```
runtime->dma_area
```

是否已经正确初始化。

------

# 九、当前结论

截至目前，`arecord` 调试结果表明：

- `arecord` 已能够正常打开 PCM Device，并进入 ALSA `hw_params()` 阶段；
- 当前驱动仅支持单声道采集；
- 采样率最终协商为 192 kHz；
- Kernel 在 `snd_pcm_hw_params()` 中发生 Oops，尚未进入 AI DMA 数据采集阶段；
- Capture 链路仍调用 ACM8816 Codec Driver，并出现 I2C NO ACK，说明当前 ASoC 拓扑仍需进一步确认；
- 目前最需要重点分析的是 **Machine Driver（DAI Link 配置）** 以及 **DMA Buffer 初始化流程**，而不是继续调整 `arecord` 参数。

因此，后续调试重点应转移到：

1. Machine Driver 中 Capture Codec 的绑定关系；
2. `msys_request_dmem()` 返回的 DMA Buffer 是否有效；
3. `runtime->dma_area` 的初始化是否符合 ALSA Core 的要求。