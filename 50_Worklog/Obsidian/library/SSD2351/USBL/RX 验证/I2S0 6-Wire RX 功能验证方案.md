## 一、项目背景

### 1. 项目目标

验证 SSD2351 的 I2S0 在配置为 **6-wire 模式**后：

- TX（Playback）能够正常工作；
  
- RX（Capture）能够正常工作；
  
- TX 与 RX 可以组成真正的 Full Duplex I2S。
  

目前 TX 已经验证成功，现在需要验证 RX。

---

## 二、当前软件配置分析

### 1. DTS 配置

目前 Device Tree 配置如下：

```dts
i2s-tx-padmux = <6>; //6-wired mode
i2s-rx-padmux = <7>; //6-wired mode

i2s-tx0-tdm-wiremode = <2>; // 1:4wire        2:6 wire
i2s-rx0-tdm-wiremode = <2>; // 1:4wire        2:6 wire
```

因此可以确认：

> I2S0 已经配置为 6-wire 模式。

---

### 2. TX/RX 主从模式

TX：

```dts
i2s-tx0-tdm-mode = <1>;
```

表示：

```
TX = Master
```

RX：

```dts
i2s-rx0-tdm-mode = <2>;
```

表示：

```
RX = Slave
```

因此：

TX 输出方向由 SSD2351 提供时钟；

RX 接收方向等待外部设备提供时钟。

---

### 3. ALSA 状态

目前：

```
cat /proc/asound/pcm
```

得到：

```
playback 1
capture 1
```

说明：

```
ALSA
↓

CPU DAI

↓

Codec DAI

↓

PCM

Capture 通路已经建立
```

软件已经支持录音。

---

## 三、目前已经完成的验证

### TX 验证

已经完成：

播放：

```
aplay
```

测试结果：

```
TX_BCLK = 3.072MHz
```

说明：

SSD2351 TX：

- Clock Generator 正常；
  
- TX FIFO 正常；
  
- DMA 正常；
  
- Codec 正常；
  
- Playback 正常。
  

因此：

TX 不需要继续验证。

---

## 四、目前最大的疑问

很多资料把 6-wire 理解成：

```
TX_BCLK

RX_BCLK

TX_LRCK

RX_LRCK
```

实际上目前没有任何证据能够证明 SSD2351 是这种结构。

目前能够确认的只有：

```
TX_DATA

RX_DATA
```

已经分离。

至于：

```
BCLK

LRCK
```

究竟：

- 共用；
  
- 还是独立；
  

目前仅靠 DTS 无法确定。

因此：

RX 的验证不能依赖：

"是否能够测到 RX_BCLK"

而应该直接验证：

CPU 是否能够正确收到 RX_DATA。

---

## 五、为什么 RX 不能只测 BCLK

因为：

当前 DTS：

```
RX = Slave
```

Slave 的定义就是：

等待外部提供：

```
BCLK

LRCK
```

因此：

如果没有：

- FPGA
  
- AK5538
  

提供 Clock，

CPU 不输出 RX Clock 是完全正常的。

因此：

"测不到 RX_BCLK"

并不能说明：

RX 坏了。

---

## 六、真正应该验证什么

真正需要验证的是：

```
FPGA

↓

I2S RX

↓

SSD2351

↓

DMA

↓

ALSA

↓

Userspace
```

整个接收链路。

验证目标：

CPU 是否能够收到 FPGA 发来的数据。

---

## 七、推荐验证方案

### 第一步

FPGA 不连接模拟部分。

FPGA 直接模拟一个 I2S ADC。

例如：

Left：

```
0x123456
```

Right：

```
0xABCDEF
```

不断循环发送。

不要发送随机数据。

---

### 第二步

FPGA 提供：

```
BCLK

LRCK

DATA
```

原因：

当前 DTS：

```
RX = Slave
```

因此：

CPU RX 等待：

```
External Clock
```

---

### 第三步

Linux：

启动：

```
arecord
```

例如：

```
arecord -D hw:0,0 \
-f S32_LE \
-r 48000 \
-c 2 \
-d 10 \
test.wav
```

---

### 第四步

观察：

```
cat /proc/interrupts
```

录音期间：

Audio DMA 中断应该不断增加。

如果：

始终没有：

说明：

RX 根本没有开始工作。

---

### 第五步

查看：

```
test.wav
```

或者：

自己写程序：

```
read()

printf("%08x")
```

如果看到：

```
123456

ABCDEF

123456

ABCDEF
```

循环出现。

即可证明：

RX 数据已经进入 CPU。

---

## 八、如果失败，应如何定位

### 情况一

录音失败。

首先：

确认：

```
arecord
```

是否真正启动。

---

### 情况二

DMA 没有中断。

说明：

RX 没有 Enable。

检查：

Driver。

---

### 情况三

DMA 正常。

但是：

全部都是：

```
0x000000
```

说明：

CPU 没收到数据。

检查：

FPGA DATA。

---

### 情况四

收到乱码。

检查：

- Sample Width
  
- I2S Mode
  
- Left Justified
  
- WS 极性
  
- BCLK 极性
  

是否一致。

---

## 九、最终判定标准

验证成功的唯一标准不是：

"测到 RX_BCLK"

而是：

CPU 能够通过 ALSA Capture 正确收到 FPGA 发来的固定 Pattern。

满足：

```
FPGA

↓

I2S RX

↓

DMA

↓

ALSA

↓

Userspace

↓

Pattern 正确
```

即可证明：

I2S0 RX 功能正常。

同时也证明：

I2S0 已经真正工作于 6-wire 模式。

---

## 十、建议的验证顺序

建议严格按照以下顺序进行：

1. 确认 ALSA 已有 Capture Device（已完成）。
   
2. 启动 arecord，确认 Capture 能够正常打开。
   
3. FPGA 输出固定 Pattern，而不是模拟音频。
   
4. FPGA 根据最终确认的主从关系提供所需的 BCLK/LRCK（当前 DTS 配置下，RX 为 Slave，应准备提供外部时钟）。
   
5. 查看 DMA 中断是否增加。
   
6. 读取 PCM 数据，验证是否与 FPGA 输出 Pattern 一致。
   
7. 数字链路验证完成后，再接入 AK5538，验证模拟音频采集。