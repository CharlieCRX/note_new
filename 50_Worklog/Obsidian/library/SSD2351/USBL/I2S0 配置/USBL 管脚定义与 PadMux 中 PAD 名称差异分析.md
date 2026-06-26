
# 1. 问题描述

在分析 USBL I2S0 管脚配置过程中，发现《USBL 水面设备管脚定义》与 `pcupid-ssm001c-s01a-voip-padmux.dtsi` 中关于 TX_BCLK、TX_WCK 的 PAD 名称并不一致。

例如：

### USBL 管脚定义

| I2S 信号 | PAD          |
| -------- | ------------ |
| TX_BCLK  | PAD_GPIOD_04 |
| TX_WCK   | PAD_GPIOD_05 |
| TX_SDO0  | PAD_GPIOD_03 |

而 padmux.dtsi 中却配置为：

```dts
<PAD_PWM_ADC00 PINMUX_FOR_I2S0_4W_TX_MODE_1 MDRV_PUSE_I2S0_TX_BCK>
<PAD_PWM_ADC01 PINMUX_FOR_I2S0_4W_TX_MODE_1 MDRV_PUSE_I2S0_TX_WCK>
<PAD_GPIOD_03  PINMUX_FOR_I2S0_4W_TX_MODE_1 MDRV_PUSE_I2S0_TX_SDO>
```

初看之下，会认为：

- USBL 要求使用 PAD_GPIOD_04；
- DTS 却配置成 PAD_PWM_ADC00；

似乎两者存在冲突。

------

# 2. ARMTmux 提供的重要信息

查阅《SSD2351X_HW Checklist》ARMTmux 页面后，发现如下信息：

| PAD_Name     | SW_Name       |
| ------------ | ------------- |
| PAD_GPIOD_04 | PAD_PWM_ADC00 |
| PAD_GPIOD_05 | PAD_PWM_ADC01 |

同时：

| PAD_Name     | I2S 功能    |
| ------------ | ----------- |
| PAD_GPIOD_04 | I2S0_TX_BCK |
| PAD_GPIOD_05 | I2S0_TX_WCK |

这里需要注意两个字段：

- **PAD_Name**
- **SW_Name**

二者并不是两颗不同的 PAD。

而更像是：

- PAD_Name：硬件 PAD 名称（Hardware Pad Name）
- SW_Name：SDK 中的软件名称（Software Name）

即：

```text
PAD_GPIOD_04
        │
        └──── SW_Name：PAD_PWM_ADC00
PAD_GPIOD_05
        │
        └──── SW_Name：PAD_PWM_ADC01
```

因此：

PAD_PWM_ADC00 并不是另一颗 PAD，而是 PAD_GPIOD_04 在软件中的命名。

------

# 3. 为什么 DTS 使用 SW_Name？

观察 padmux.dtsi 可以发现：

```dts
<PAD_PWM_ADC00
 PINMUX_FOR_I2S0_4W_TX_MODE_1
 MDRV_PUSE_I2S0_TX_BCK>
```

如果：

```text
PAD_PWM_ADC00
```

就是：

```text
PAD_GPIOD_04
```

的软件名称，那么：

上面的 DTS 实际上等价于：

```text
PAD_GPIOD_04
        │
        ▼
I2S0_TX_BCK
```

因此：

USBL 文档中的：

```text
PAD_GPIOD_04
```

与 DTS 中的：

```text
PAD_PWM_ADC00
```

并不是冲突关系，而是：

**同一颗 PAD 的两种命名方式。**

同理：

```text
PAD_PWM_ADC01
```

对应：

```text
PAD_GPIOD_05
```

用于：

```text
I2S0_TX_WCK
```

------

# 4. 当前分析结论

目前已有证据表明：

| USBL 文档    | ARMTmux                 | DTS           |
| ------------ | ----------------------- | ------------- |
| PAD_GPIOD_04 | SW_Name = PAD_PWM_ADC00 | PAD_PWM_ADC00 |
| PAD_GPIOD_05 | SW_Name = PAD_PWM_ADC01 | PAD_PWM_ADC01 |
| PAD_GPIOD_03 | PAD_GPIOD_03            | PAD_GPIOD_03  |

因此：

目前不能仅凭 PAD 名称不同，就认为 DTS 配置错误。

更合理的解释是：

> USBL 文档采用的是 **Hardware Pad Name（PAD_Name）**；而 DTS 使用的是 **Software Name（SW_Name）**。

二者引用的是同一颗物理 PAD。

------

# 5. 仍需进一步验证的内容

虽然目前名称差异已经得到合理解释，但仍需进一步确认以下两个问题。

## （1）确认 SW_Name 与 PAD_Name 的映射关系

建议继续查看：

- padmux.h
- mdrv_padmux.h
- pinmux 定义文件

确认：

```c
PAD_PWM_ADC00
```

是否最终对应：

```text
PAD_GPIOD_04
```

以及：

```c
PAD_PWM_ADC01
```

是否对应：

```text
PAD_GPIOD_05
```

若能够确认两者使用相同的 PAD 编号，则可以完全证明：

> DTS 与 USBL 文档实际上引用的是同一颗 PAD。

------

## （2）确认 PadMux Mode 的选择过程

目前 DTS 中使用：

```text
PINMUX_FOR_I2S0_4W_TX_MODE_1
```

但真正决定写入哪一个 PadMux Mode 的，并不是 padmux.dtsi，而是：

```text
pcupid.dtsi
        │
        ▼
i2s-tx-padmux = <6>
        │
        ▼
PadMux Driver
        │
        ▼
PINMUX_FOR_I2S0_4W_TX_MODE_x
```

因此下一步建议分析：

- PadMux Driver 如何解析 `i2s-tx-padmux=<6>`；
- 最终写入哪个 PadMux Register；
- 与 ARMTmux 中的 Mode1～Mode4 是否完全一致。

------

# 6. 当前阶段结论

截至目前分析，可以得到如下结论：

1. USBL 文档与 DTS 中 PAD 名称不同，并不能直接说明配置错误。
2. ARMTmux 已经表明 PAD_Name 与 SW_Name 存在对应关系。
3. DTS 使用的是 SW_Name，而 USBL 文档使用的是 PAD_Name。
4. 当前更值得继续验证的是 **PadMux Driver 如何根据 `i2s-tx-padmux=<6>` 选择具体的 PadMux Mode**，以及最终是否正确配置了对应的寄存器。

因此，目前没有足够证据证明 TX_BCLK、TX_WCK 的 PadMux 配置存在错误，更大的可能是不同文档采用了不同的 PAD 命名体系，而实际引用的是同一颗物理 PAD。