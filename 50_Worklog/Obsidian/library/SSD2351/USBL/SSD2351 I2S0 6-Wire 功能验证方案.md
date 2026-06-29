# 1. 验证目标

经过前期分析，已经确认：

- USBL 管脚定义与 ARMTmux 一致；
  
- ARMTmux 中 PAD_Name 与 SW_Name 可以一一对应；
  
- PadMux 配置与 PCB 硬件连接一致；
  
- Device Tree 已配置为 I2S0 6-Wire 模式。
  

因此，本阶段**不再验证配置是否正确**，而是验证：

> **SSD2351 是否真正工作在 I2S0 6-Wire 模式，并能够实现 TX 与 RX 同时独立工作。**

需要验证的问题包括：

1. TX 通道是否正常输出？
   
2. RX 通道是否正常接收？
   
3. TX 与 RX 是否能够同时工作？
   
4. TX 与 RX 是否真正拥有独立的时钟，而不是共享时钟？
   

---

# 2. I2S0 6-Wire 的验证思路

需要注意的是：

**6-Wire 的本质不是 TX 能工作，也不是 RX 能工作，而是两条物理链路能够同时独立工作。**

典型的 6-Wire 连接如下：

```text
                 SSD2351

          TX                      RX

TX_BCLK  ------------->

TX_WCK   ------------->

TX_SDO   ------------->

                     <------------- RX_BCLK

                     <------------- RX_WCK

                     <------------- RX_SDI
```

因此：

整个验证应围绕：

**六根 I2S 信号是否真正工作** 展开。

---

# 3. 为什么不能只验证 Playback？

很多应用验证仅仅做到：

```text
acm8816Launch

↓

播放成功
```

这只能证明：

```text
Userspace
      │
ALSA
      │
Playback
```

能够工作。

但是：

不能证明：

- TX_BCLK 是否真正输出；
  
- RX_BCLK 是否存在；
  
- TX 与 RX 是否能够同时工作；
  
- 是否真正进入 6-Wire 模式。
  

因此：

不能仅依据：

播放成功，

判断：

I2S0 已支持 6-Wire。

---

# 4. 推荐的验证流程

建议按照：

由浅入深、

逐层验证、

逐步排除问题

的方式进行。

整个实验建议分为：

四个阶段。

---

# 第一阶段：验证 TX 输出

## 验证目的

确认：

CPU

已经真正开始发送：

I2S TX 数据。

推荐：

运行：

```text
acm8816Launch
```

发送：

例如：

- 48kHz
  
- Stereo
  
- 16bit
  
- 1kHz 正弦波
  

作为固定测试信号。

---

## 示波器测量信号

建议测量：

|信号|作用|
|---|---|
|TX_BCLK|Bit Clock|
|TX_WCK（LRCK）|Word Clock|
|TX_SDO|Serial Data|

如果能够测量：

MCLK，

建议同时测量。

---

## 验证标准

### TX_WCK

理论：

```text
TX_WCK = SampleRate
```

例如：

播放：

```text
48kHz
```

则：

示波器应测得：

```text
≈48kHz
```

原因：

LRCK（Word Clock）每发送一个采样点，

翻转一次。

因此：LRCK 的频率永远等于：采样率。

---

### TX_BCLK

理论：

```text
BCLK = SampleRate × SlotWidth × Channel
```

例如：

48kHz

Stereo

32bit Slot

则：

```text
48000 × 32 × 2

=

3072000Hz
```

即：

```text
≈3.072MHz
```

如果：

16bit Slot，

则：

```text
≈1.536MHz
```

因此通过 BCLK：

即可判断 Driver 采用：

16bit

还是：

32bit Slot。

---

### TX_SDO

播放：

正弦波：

应持续变化。

播放：

静音：

可能保持低电平。

因此：

SDO：

主要用于：

辅助判断：

PCM 是否真正输出。

---

## 第一阶段完成标准

满足：

- TX_BCLK 正常；
  
- TX_WCK 正常；
  
- TX_SDO 持续输出；
  

说明：

TX 数据链路已经打通。

---

# 第二阶段：验证 RX 输入

## 验证目的

确认：

CPU

能够正常接收：

I2S RX 数据。

需要：

让：

Codec

进入：

ADC Capture。

然后：

SSD2351

开始：

录音。

建议：

使用：

```text
arecord
```

或者：

Capture API。

---

## 示波器测量

测量：

|信号|说明|
|---|---|
|RX_BCLK|Bit Clock|
|RX_WCK|Word Clock|
|RX_SDI|Data Input|

---

## 验证标准

RX_BCLK：

持续输出。

RX_WCK：

应等于：

SampleRate。

RX_SDI：

存在：

PCM 数据。

若：

三项全部正常，

说明：

RX 数据链路已经建立。

---

# 第三阶段：验证 TX 与 RX 是否独立

这是：

整个实验：

最关键的一步。

建议：

使用：

8 通道示波器

或：

逻辑分析仪。

同时测量：

```text
TX_BCLK

TX_WCK

TX_SDO

RX_BCLK

RX_WCK

RX_SDI
```

然后：

同时：

启动：

Playback

Capture。

---

## 验证目标

观察：

TX

与

RX

是否：

同时存在。

例如：

Playback：

开始以后：

TX_BCLK

一直输出。

Capture：

开始以后：

RX_BCLK

也开始输出。

若：

启动：

Capture

以后：

TX 时钟：

消失。

说明：

TX

RX

仍然：

共享。

不是：

真正：

6-Wire。

---

## 验证通过标准

同时观察到：

```text
TX_BCLK

TX_WCK

TX_SDO

RX_BCLK

RX_WCK

RX_SDI
```

全部：

稳定工作。

即可证明：

TX

与

RX

真正：

独立。

---

# 第四阶段：验证真正的数据流

除了：

时钟，

还需要：

验证：

数据。

建议：

例如：

TX：

播放：

```text
1kHz
```

正弦波。

同时：

RX：

输入：

```text
2kHz
```

正弦波。

如果：

逻辑分析仪：

解码：

I2S。

应观察到：

TX

与

RX

PCM

完全不同。

说明：

两条：

数据链路：

互不影响。

真正：

独立工作。

---

# 5. 推荐使用的测试设备

不同设备：

验证能力：

不同。

|工具|可验证内容|推荐程度|
|---|---|---|
|万用表|电压、电源|★☆☆☆☆|
|示波器|时钟、电平、频率|★★★★★|
|逻辑分析仪|I2S 解码、PCM 数据|★★★★★|
|ALSA|Playback/Capture 是否正常打开|★★★★☆|
|/proc/interrupts|DMA 是否工作|★★★★☆|

其中：

**示波器**

用于：

验证：

物理层。

逻辑分析仪：

用于：

验证：

协议层。

二者：

结合：

效果最佳。

---

# 6. 推荐实验顺序

建议：

严格按照：

下面顺序进行。

**Step1**

启动：

Playback。

测量：

```text
TX_BCLK

TX_WCK

TX_SDO
```

验证：

TX。

---

**Step2**

启动：

Capture。

测量：

```text
RX_BCLK

RX_WCK

RX_SDI
```

验证：

RX。

---

**Step3**

同时：

启动：

Playback

Capture。

同时：

测量：

```text
TX_BCLK

TX_WCK

TX_SDO

RX_BCLK

RX_WCK

RX_SDI
```

验证：

TX

RX

是否：

真正：

同时工作。

---

**Step4**

使用：

逻辑分析仪：

解码：

TX

与

RX。

验证：

PCM 数据。

---

# 7. 最终判断标准

建议：

按照如下标准判断：

|验证项目|验证方法|判定标准|
|---|---|---|
|TX 时钟|示波器|TX_BCLK、TX_WCK 正常输出|
|TX 数据|示波器/逻辑分析仪|TX_SDO 输出 PCM 数据|
|RX 时钟|示波器|RX_BCLK、RX_WCK 正常输出|
|RX 数据|示波器/逻辑分析仪|RX_SDI 接收到 PCM 数据|
|TX+RX 同时工作|Playback+Capture|六根 I2S 信号同时存在|
|数据独立|逻辑分析仪解码|TX 与 RX PCM 数据互不影响|

---

# 8. 实验完成后的结论

如果上述所有实验均满足预期，则可以得出如下结论：

1. SSD2351 的 I2S0 TX 通道能够正常输出 I2S 数据。
   
2. SSD2351 的 I2S0 RX 通道能够正常接收 I2S 数据。
   
3. TX 与 RX 具有各自独立的 BCLK 与 WCK，不存在时钟共享现象。
   
4. TX 与 RX 可以同时工作，且互不影响。
   
5. 在当前硬件连接、PadMux 配置以及 Device Tree 配置下，SSD2351 已成功工作于 **I2S0 6-Wire 全双工模式**，能够满足同步收发应用需求。