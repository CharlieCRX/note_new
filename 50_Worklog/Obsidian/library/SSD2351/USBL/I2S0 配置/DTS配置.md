------

# 一、先建立整个 Linux 的配置链

实际上，一路的数据流是这样的：

```text
               PCB原理图
                    │
                    │（硬件已经焊死）
                    ▼
《USBL水面设备管脚定义》
                    │
      告诉你 I2S0 应该接哪些 PAD
                    │
                    ▼
 ARMTmux(CheckList)
                    │
     告诉你每个 PAD 的寄存器模式
                    │
                    ▼
pcupid-ssm001c-s01a-voip-padmux.dtsi
                    │
      Linux描述 "我要选择哪一种PadMux"
                    │
                    ▼
Padmux Driver
                    │
      写 PadMux Register
                    │
                    ▼
PAD真正变成I2S功能
                    │
                    ▼
pcupid.dtsi
                    │
      配置 I2S 控制器(BACH)
                    │
                    ▼
sstar_bach.c
                    │
      初始化 I2S Controller
                    │
                    ▼
I2S开始收发数据
```

所以实际上：

> **PadMux 和 I2S Controller 是两件事情。**

很多人第一次都会把它们混在一起。

------

# 二、《USBL 管脚定义》负责什么？

例如你的文档写的是：

| PAD          | 信号    |
| ------------ | ------- |
| PAD_GPIOD_03 | TX_SDO0 |
| PAD_OUTP_CH2 | RX_BCLK |
| PAD_OUTN_CH2 | RX_WCK  |
| PAD_OUTP_CH3 | RX_SDI  |

这只是说明：

```text
CPU I2S0 TX_SDO
        │
        ▼
PAD_GPIOD_03
```

或者

```text
CPU RX_BCLK
      │
      ▼
PAD_OUTP_CH2
```

它**只是硬件连接关系**。

它不会告诉 Linux：

> "请选择这个功能。"

------

# 三、ARMTmux 又负责什么？

例如 Checklist 里面有一行：

```text
PAD_GPIOD_03

reg_i2s0_4w_tx_mode
```

意思就是：

这个 PAD 有很多功能：

```text
PAD_GPIOD_03

      │
      ├── GPIO
      ├── UART
      ├── SPI
      ├── LCD
      └── I2S0_TX_SDO
```

而：

```text
reg_i2s0_4w_tx_mode
```

控制：

到底选哪个。

所以：

ARMTmux 实际上就是：

> **PinMux 寄存器说明书。**

------

# 四、Linux 如何使用这个说明书？

重点来了。

Linux 并不会直接写：

```c
reg_i2s0_4w_tx_mode=4;
```

而是：

在 DTS 里面写。

你上传的：

```text
pcupid-ssm001c-s01a-voip-padmux.dtsi
```

实际上就是：

**Linux 对 ARMTmux 的封装。**

例如：

```dts
<PAD_OUTP_CH2
 PINMUX_FOR_I2S0_RX_MODE_3
 MDRV_PUSE_I2S0_RX_BCK>
```

这一句话拆开来看。

第一列：

```text
PAD_OUTP_CH2
```

就是：

哪一个 PAD。

第二列：

```text
PINMUX_FOR_I2S0_RX_MODE_3
```

就是：

ARMTmux 里面说的：

> 设置哪个寄存器模式。

第三列：

```text
MDRV_PUSE_I2S0_RX_BCK
```

表示：

这个 PAD 的用途。

所以：

这一行实际上可以翻译成人话：

> 把 PAD_OUTP_CH2 配置成 I2S0_RX_BCK。

------

例如：

```dts
<PAD_GPIOD_03
 PINMUX_FOR_I2S0_4W_TX_MODE_4
 MDRV_PUSE_I2S0_TX_SDO>
```

翻译就是：

> 把 PAD_GPIOD_03 配置成 I2S0 的 TX_SDO。

是不是发现：

它其实就是：

ARMTmux 的 Linux 版本。

------

# 五、那 pinctrl 到底是什么？

很多人会误认为：

```text
PadMux = pinctrl
```

其实不是。

Linux 的 pinctrl 是：

**Pin Controller Framework。**

它负责：

```text
Device Driver
      │
      ▼
 pinctrl
      │
      ▼
 PadMux Driver
      │
      ▼
 Register
```

所以：

pinctrl 是：

> Linux 内核的一套统一接口。

例如：

驱动写：

```c
devm_pinctrl_get();
```

Linux：

找到：

```dts
pinctrl-0 = <&i2s0_pins>;
```

然后：

```text
Padmux Driver
```

去：

```text
写寄存器
```

完成：

PinMux。

所以：

```text
Driver
   │
   ▼
pinctrl Framework
   │
   ▼
PadMux Driver
   │
   ▼
Register
```

这就是 pinctrl。

------

# 六、你的平台为什么几乎看不到 pinctrl？

这是 SigmaStar 平台比较特殊的地方。

我查看了你上传的 DTS。

发现：

真正的：

```text
padmux {
```

就在：

```text
pcupid-ssm001c-s01a-voip-padmux.dtsi
```

例如：

```dts
padmux {
    compatible = "sstar,padmux";
```

也就是说：

SigmaStar 没有采用 Linux Mainline 那种：

```dts
&pinctrl {

    i2s0_pins {
    };

};
```

而是：

自己实现了一套：

```text
PadMux Driver
```

它读取：

```dts
padmux {
    ...
}
```

然后：

直接：

```text
写寄存器。
```

所以：

这里：

```text
padmux节点
```

实际上就是：

SigmaStar 自己实现的：

> pinctrl provider。

------

# 七、真正控制 I2S 的又是谁？

注意：

PadMux 配好了以后：

**I2S 还不会工作。**

因为：

PadMux 只是：

```text
让电线接通
```

真正产生：

```text
BCLK

LRCK

SDO

SDI
```

的是：

```text
I2S Controller
```

而它就在：

```text
pcupid.dtsi
```

例如：

```dts
i2s-tx0-tdm-wiremode = <2>;
```

这个不是 PadMux，它是 I2S Controller 配置。

例如：

```dts
i2s-tx0-tdm-mode = <1>;
```

表示 Master。

例如：

```dts
i2s-rx0-tdm-mode = <2>;
```

表示 Slave。

例如：

```dts
i2s-rx0-tdm-wiremode = <2>;
```

表示：

```text
6-wire
```

例如：

```dts
i2s-rx0-channel=<2>;
```

表示：

2 channel。

这些：

都会进入：

```text
sstar_bach.c
```

驱动。

然后：

```text
写 I2S Controller Register
```

------

# 八、你的 DTS 已经透露了一个非常重要的信息

我在 `pcupid.dtsi` 中看到：

```dts
i2s-trx-shared-padmux = <0>; //for 4-wired mode panmux
i2s-tx-padmux = <6>;         //6-wired mode
i2s-rx-padmux = <7>;         //6-wired mode
i2s-mck-padmux = <7>;
```

以及：

```dts
i2s-tx0-tdm-wiremode = <2>; // 2 = 6 wire
i2s-rx0-tdm-wiremode = <2>; // 2 = 6 wire
```

这说明**平台的设计是把"管脚复用"和"I2S 控制器模式"分开配置的**：

- `i2s-tx-padmux = <6>`、`i2s-rx-padmux = <7>` 用来告诉 PadMux 驱动："请选择 6-wire 所对应的 TX/RX 引脚组。"
- `i2s-tx0-tdm-wiremode = <2>`、`i2s-rx0-tdm-wiremode = <2>` 则告诉 BACH(I2S 控制器)："控制器按照 6-wire 协议工作。"

**两者缺一不可。**

------

# 九、结合你的 I2S0 6-wire 验证，我建议后续的分析顺序

我们已经知道了硬件连接和 DTS 的整体框架，下一步就应该沿着 Linux 驱动的初始化路径继续追踪，而不是再停留在 DTS。

建议按照下面这条链逐级验证：

```text
USBL 管脚定义
      │
      ▼
ARMTmux(CheckList)
      │
      ▼
pcupid-ssm001c-s01a-voip-padmux.dtsi
      │
      ▼
PadMux Driver（真正写 PadMux 寄存器）
      │
      ▼
pcupid.dtsi（BACH 属性）
      │
      ▼
sstar_bach.c（解析 Device Tree）
      │
      ▼
I2S Controller Register（配置 Master/Slave、4-wire/6-wire、Channel 等）
      │
      ▼
BCLK / WCK / SDO / SDI 在对应物理管脚上输出或输入
```

**我建议下一步我们直接分析 `sstar_bach.c`：**从 `probe()` 开始，一路跟踪 `of_property_read_*()` 如何读取你 DTS 中的 `i2s-tx-padmux`、`i2s-rx-padmux`、`i2s-tx0-tdm-wiremode` 等属性，并最终写入哪些硬件寄存器。这样你就能完整理解 **Device Tree → 驱动 → 寄存器 → I2S0 6-wire 生效** 的整个链路，这也是 Linux ASoC 驱动调试最重要的一步。