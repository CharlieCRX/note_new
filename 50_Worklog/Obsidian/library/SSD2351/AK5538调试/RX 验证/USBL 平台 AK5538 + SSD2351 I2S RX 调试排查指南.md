# USBL 平台 AK5538 + SSD2351 I2S RX 调试排查指南

**版本**：V1.0
**适用平台**：USBL（SSD2351 + FPGA + AK5538）
**目的**：定位 AK5538 → SSD2351 I2S RX → ALSA Capture 无法录音的问题。

------

# 1. 系统架构

根据硬件框图，整个采集链路如下：

```text
                           FPGA
                (MCLK配置、AD复位)
                            │
                            │ MCLK
                            ▼
                    ┌────────────────┐
模拟输入 ─────────► │   AK5538 ADC    │
                    │                │
                    └────────────────┘
                            │
                     I2S (SDTO)
                            │
             BCLK / LRCK ← SSD2351(I2S Master)
                            │
                            ▼
                    SSD2351 I2S RX
                            │
                            ▼
                        AI DMA(MHAL)
                            │
                            ▼
                     ALSA PCM Capture
                            │
                            ▼
                         arecord
```

整个系统可以划分为 **6 个验证层次**。

> **原则：**
>
> **上一层未通过，不进入下一层。**
>
> 否则容易陷入软件、驱动、硬件交叉排查，效率极低。

------

# 2. AK5538 工作原理

AK5538 是 **8 通道 Audio ADC**。

它不是 I2S Master，而是 **I2S Slave**。

因此：

**AK5538 不产生 MCLK/BCLK/LRCK。**

所有时钟均来自外部。

------

## 2.1 三个时钟作用

### MCLK（Master Clock）

作用：

- ADC 内部工作时钟
- Sigma-Delta 调制器
- 数字滤波器
- 内部数字逻辑

例如：

```
48kHz

↓

MCLK = 256Fs

↓

12.288MHz
```

如果没有 MCLK：

```
ADC 不工作

↓

不会产生采样数据
```

------

### LRCK（Word Clock）

也称：

```
FS
```

作用：

决定采样率。

例如：

```
LRCK = 48kHz

↓

ADC 每秒输出 48000 个 Sample
```

------

### BCLK（Bit Clock）

作用：

控制 I2S 数据移位。

例如：

```
48kHz

×

64bit

=

3.072MHz
```

因此：

```
BCLK = LRCK × FrameBits
```

------

## 三者关系

```
           FPGA
             │
             │ MCLK
             ▼
         AK5538内部ADC
             │
             ▼
       等待LRCK开始采样
             │
             ▼
       等待BCLK发送数据
             │
             ▼
           SDTO输出
```

因此：

AK5538 必须满足：

```
MCLK

+

LRCK

+

BCLK
```

三者同时存在，

才能输出：

```
SDTO
```

------

# 3. 配置完成以后，应验证什么？

你的 Shell 脚本已经完成：

✓ FPGA MCLK 配置

✓ FPGA AD Reset

✓ AK5538 I2C 配置

但是：

**寄存器配置成功 ≠ ADC 已开始输出数据。**

因此需要继续验证。

------

# 4. 推荐排查流程

整个调试建议按照下面流程进行。

```
FPGA

↓

AK5538

↓

SSD2351 I2S

↓

AI DMA

↓

ALSA

↓

arecord
```

具体分为六层。

------

# 第一层：FPGA 是否正常工作

## 目标

确认 FPGA 已经输出 ADC 工作时钟。

### 已完成

Shell：

```
fpga -w 0x2 xx
```

配置：

```
MCLK
```

### 应测量

示波器：

```
AK5538 MCLK
```

应看到：

```
12.288MHz

16.384MHz

24.576MHz
```

根据配置不同而变化。

### 若失败

说明：

```
FPGA

↓

MCLK配置错误
```

无需继续。

------

# 第二层：AK5538 是否进入工作状态

Shell：

```
i2cset
```

已经配置：

```
Power_Ma1

Power_Ma2

Control_1
```

建议：

回读：

```
i2cget
```

确认：

```
Power

Control_1
```

均正确。

同时：

测量：

```
PDN
```

确认：

```
High
```

说明：

ADC 已解复位。

------

## 第二层通过标准

```
PDN High

+

MCLK存在

+

寄存器回读正确
```

------

# 第三层：SSD2351 是否真正启动 I2S RX

这是目前重点。

执行：

```
arecord
```

期间：

测量：

```
CPU MCLK

CPU BCLK

CPU LRCK
```

重点：

```
BCLK

LRCK
```

如果：

```
没有 BCLK

没有 LRCK
```

说明：

```
I2S RX

没有启动
```

此时：

AK5538 不可能输出数据。

------

## 第三层通过标准

```
BCLK

正常

+

LRCK

正常
```

------

# 第四层：AK5538 是否输出 SDATA

只有：

```
MCLK

+

BCLK

+

LRCK
```

全部存在，

AK5538：

才开始：

```
SDTO
```

建议：

示波器：

```
AK5538 SDTO
```

如果：

```
一直高

一直低

Hi-Z
```

说明：

ADC 仍未输出数据。

------

## 第四层通过标准

```
SDTO

有连续数据
```

------

# 第五层：AI DMA 是否启动

目前日志：

```
mhal_audio_ai_start()

↓

bIsAttached FALSE
```

说明：

```
AI DMA

没有Attach
```

当前重点检查：

```
mhal_audio_ai_attach()
```

是否真正执行。

建议：

打印：

```
before attach

after attach

ret
```

查看：

```
Attach

是否成功。
```

------

## 第五层通过标准

日志：

```
bIsAttached TRUE
```

------

# 第六层：ALSA 是否收到 PCM

最后：

```
arecord
```

能够生成：

```
test.wav
```

使用：

```
hexdump

Audacity

ffplay
```

查看：

是否真正收到 PCM 数据。

------

# 5. 当前问题定位

结合目前所有现象：

```
I2C

正常
```

↓

```
AK5538

寄存器配置正常
```

↓

```
arecord

开始
```

↓

```
mhal_audio_ai_start()

↓

bIsAttached FALSE
```

↓

```
没有BCLK

没有LRCK
```

↓

```
AK5538

没有SDTO输出
```

↓

```
Bad address
```

因此：

**当前问题最有可能发生在 SSD2351 I2S RX 初始化阶段，而不是 AK5538 本身。**

------

# 6. 推荐每天调试顺序

建议以后所有 RX 调试，都按照下面顺序。

------

## 第一步

执行：

```
test_ak5538.sh
```

确认：

```
I2C

MCLK

Power
```

均正常。

------

## 第二步

示波器：

测：

```
AK5538 MCLK
```

确认：

```
12.288MHz

24.576MHz
```

------

## 第三步

执行：

```
arecord
```

同时：

测：

```
SSD2351

BCLK

LRCK
```

------

## 第四步

若：

```
BCLK

LRCK

正常
```

继续测：

```
AK5538 SDTO
```

------

## 第五步

若：

```
SDTO

正常
```

检查：

```
mhal_audio_ai_attach()

↓

bIsAttached
```

------

## 第六步

最后：

确认：

```
arecord

是否生成PCM
```

------

# 7. 调试流程图

```text
                 FPGA 配置
                     │
                     ▼
          ① FPGA 输出 MCLK？
                     │
          ┌──────────┴──────────┐
          │                     │
         否                    是
          │                     │
   检查 FPGA                 ▼
                     ② AK5538 工作？
              (PDN、寄存器、Power)
                     │
          ┌──────────┴──────────┐
          │                     │
         否                    是
          │                     │
   检查 AK5538             ▼
                ③ SSD2351 输出 BCLK/LRCK？
                     │
          ┌──────────┴──────────┐
          │                     │
         否                    是
          │                     │
检查 I2S RX / AI DMA         ▼
                  ④ AK5538 输出 SDTO？
                     │
          ┌──────────┴──────────┐
          │                     │
         否                    是
          │                     │
检查 AK5538 配置            ▼
                  ⑤ AI DMA Attach？
                     │
          ┌──────────┴──────────┐
          │                     │
         否                    是
          │                     │
检查 mhal_audio_ai_attach()  ▼
                  ⑥ ALSA 收到 PCM？
                     │
          ┌──────────┴──────────┐
          │                     │
         否                    是
          │                     │
检查 PCM/DMA             RX 验证完成
```

------

# 8. 当前建议的下一步

根据目前已有的调试结果，我建议按下面顺序继续验证：

1. **示波器确认 AK5538 的 MCLK**：验证 FPGA 输出是否真正到达 AK5538。
2. **运行 `arecord` 的同时测量 SSD2351 的 BCLK、LRCK**：确认 I2S RX 是否真正启动。
3. **如果 BCLK/LRCK 正常，再测量 AK5538 的 SDTO**：判断 AK5538 是否开始输出数字音频数据。
4. **如果 BCLK/LRCK 不正常，则集中分析驱动初始化**：继续跟踪 `mhal_audio_ai_attach()`、`mhal_audio_ai_start()` 和 `bIsAttached` 的状态，而无需继续怀疑 AK5538 本身。

这种由**硬件 → 接口 → 驱动 → ALSA**的排查方式，可以避免不同层次的问题互相干扰，更容易快速定位根因。