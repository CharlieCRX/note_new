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

