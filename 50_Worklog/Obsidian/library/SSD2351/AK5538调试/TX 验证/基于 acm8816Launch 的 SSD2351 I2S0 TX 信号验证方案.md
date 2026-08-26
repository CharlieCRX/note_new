# 1. 验证目的

本实验用于验证 SSD2351 的 I2S0 TX 数据链路是否真正工作。

验证对象包括：

- TX_BCLK（Bit Clock）
  
- TX_WCK（LRCK / Word Clock）
  
- TX_SDO（Serial Data）
  
- （可选）MCLK（Master Clock）
  

通过示波器测量上述信号，验证：

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
SSD2351 I2S0 TX Pin
```

整条发送链路是否正常。

---

# 2. 推荐测试命令

建议固定采用如下参数：

```bash
./acm8816Launch -f 1000 -R 48000 -b 32 -t 10000 -v 19
```

参数说明：

|参数|值|作用|
|---|---|---|
|-f|1000|输出 1kHz 正弦波|
|-R|48000|采样率 48kHz|
|-b|32|32bit Sample（便于验证 Slot Width）|
|-t|10000|持续发送 10 秒|
|-v|19|默认音量|

选择上述参数的原因：

- 48kHz 是最常见音频采样率；
  
- 32bit Slot 是 Linux ASoC 常见配置；
  
- 1kHz 正弦波容易观察 PCM 数据变化。
  

---

# 3. I2S TX 管脚

根据前期分析：

| I2S 信号  | PAD          |
| ------- | ------------ |
| TX_BCLK | PAD_GPIOD_04 |
| TX_WCK  | PAD_GPIOD_05 |
| TX_SDO  | PAD_GPIOD_03 |
| MCLK    | PAD_OUTN_CH1 |

建议：

示波器至少测量：

- TX_BCLK
  
- TX_WCK
  
- TX_SDO
  

若条件允许：

同时测量：

MCLK。

---

# 4. 每个管脚应该观察什么？

## 4.1 TX_WCK（LRCK）

### 作用

Word Clock。

表示：

当前开始发送一个新的采样点。

左右声道：

交替发送。

```text
L R L R L R
```

每发送：

一个采样。

LRCK：

翻转一次。

因此：

LRCK：

频率：

始终等于：

采样率。

---

### 理论值

当前：

```text
SampleRate

=

48000Hz
```

因此：

```text
TX_WCK

≈48kHz
```

---

### 为什么？

因为：

每秒：

发送：

48000 个采样。

LRCK：

每开始：

一个采样。

切换一次。

因此：

```text
LRCK = SampleRate
```

这属于：

I2S 协议规定。

与：

Codec

无关。

与：

CPU

无关。

---

### 判断标准

若：

示波器：

测得：

```text
≈48kHz
```

说明：

I2S：

采样率：

正确。

---

# 4.2 TX_BCLK

### 作用

Bit Clock。

表示：

发送：

每一个 Bit。

所需：

时钟。

---

### 理论计算

计算公式：

```text
BCLK

=

SampleRate

×

SlotWidth

×

Channel
```

其中：

当前：

```text
SampleRate

=

48000
```

Slot：

```text
32bit
```

Channel：

```text
Stereo

=

2
```

因此：

```text
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
TX_BCLK

≈3.072MHz
```

---

### 若 Driver 使用 16bit Slot

则：

```text
48000

×

16

×

2

=

1536000Hz
```

即：

```text
≈1.536MHz
```

因此：

BCLK：

不仅能够验证：

I2S 是否启动。

还能判断：

Driver：

采用：

16bit Slot

还是：

32bit Slot。

---

### 判断标准

若：

示波器：

测得：

约：

```text
3.072MHz
```

说明：

Driver：

采用：

32bit Slot。

若：

约：

```text
1.536MHz
```

说明：

Driver：

采用：

16bit Slot。

---

# 4.3 TX_SDO

### 作用

Serial Data Out。

用于：

发送：

PCM 数据。

---

### 当前实验

播放：

```text
1kHz

Sine
```

因此：

SDO：

应持续变化。

不能：

长期：

保持：

高电平

或：

低电平。

---

### 注意

SDO：

不能用于：

判断：

采样率。

只能：

辅助判断：

PCM

是否：

真正发送。

---

# 4.4 MCLK（可选）

若：

板卡：

输出：

MCLK。

建议：

同时测量。

通常：

48kHz：

系统：

MCLK：

可能为：

```text
12.288MHz
```

或者：

```text
24.576MHz
```

具体：

取决于：

Codec

PLL。

MCLK：

主要用于：

判断：

Codec：

是否：

获得：

参考时钟。

---

# 5. 推荐示波器连接

建议：

使用：

四通道示波器。

例如：

|通道|测量对象|
|---|---|
|CH1|TX_BCLK|
|CH2|TX_WCK|
|CH3|TX_SDO|
|CH4|MCLK（可选）|

若：

仅：

三通道。

优先：

测：

- TX_BCLK
  
- TX_WCK
  
- TX_SDO
  

---

# 6. 推荐测量顺序

## Step1

运行：

```bash
./acm8816Launch -f 1000 -R 48000 -b 32 -t 10000 -v 19
```

保持：

持续发送。

---

## Step2

测量：

TX_WCK。

确认：

约：

```text
48kHz
```

---

## Step3

测量：

TX_BCLK。

确认：

约：

```text
3.072MHz
```

若：

约：

```text
1.536MHz
```

说明：

Driver：

使用：

16bit Slot。

---

## Step4

测量：

TX_SDO。

确认：

持续变化。

说明：

PCM：

正常发送。

---

## Step5

若：

能够：

测量：

MCLK。

确认：

是否：

输出：

稳定时钟。

---

# 7. 验证结果判断

|测量项|理论值|验证内容|
|---|---|---|
|TX_WCK|≈48kHz|SampleRate 正确|
|TX_BCLK|≈3.072MHz（32bit）或≈1.536MHz（16bit）|SlotWidth 配置正确|
|TX_SDO|持续变化|PCM 数据正常发送|
|MCLK|12.288MHz / 24.576MHz（依系统配置）|Codec 主时钟正常|

---

# 8. 本阶段能够证明什么？

如果：

上述四项：

均符合理论值。

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
SSD2351 TX Pin
```

整条：

I2S0 TX 数据链路已经完全建立。

需要注意的是：

**本实验仅验证 TX 通路。**

它能够证明：

SSD2351 已能够正确发送 I2S 数据。

但仍不能证明：

I2S0 已真正工作于 6-Wire 全双工模式。

后续仍需继续验证：

- RX_BCLK
  
- RX_WCK
  
- RX_SDI
  

以及：

Playback 与 Capture 同时运行时，

TX 与 RX 是否能够保持独立工作。