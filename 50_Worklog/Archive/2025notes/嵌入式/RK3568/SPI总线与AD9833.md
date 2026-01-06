# RK3568 SPI 总线与 AD9833

本篇文章，是围绕一个最终目的来写的：

> 在 RK3568 的 SPI 总线上，能够正常读取到 AD9833 设备，并且能够正常读写其寄存器

记录下自己第一次实现 SPI 总线配置、读取设备的过程。

## 背景知识

进行探索问题的时候，基于当前本人的技术栈（C开发），需要补充大量的软硬件知识。这里汇总下，帮助更好地理解材料。

### GPIO

这是芯片上的通用引脚，基本作用：

- 输入模式：引脚可以用来接收外部信号
- 输出模式：引脚可以输出高低电平，可以用于驱动 LED、继电器等。

可以将 GPIO 理解为，芯片外接出来的一条“可编程的导线”，通过软件配置，可以将其作为输入或者输出。

但是对于每个芯片来说，通用引脚基于物理限制，数量总是有限的。为了解决实现丰富功能的理想和贫瘠的引脚数量的矛盾，芯片厂商会让同一个物理引脚能够实现不同的功能，这就是引脚复用。例如：

- 某个引脚默认是 GPIO，可以读/写高低电平；

- 但通过芯片的 **复用配置寄存器**，也可以把这个引脚切换成 **SPI_MISO**、**I2C_SDA**、**UART_TX** 等专用外设功能。

这就是“GPIO 复用”。

### SPI 模式

SPI 模式由 **CPOL + CPHA** 决定：

| 参数                      | 含义                                                         |
| ------------------------- | ------------------------------------------------------------ |
| **CPOL (Clock Polarity)** | 时钟空闲电平：0=低电平空闲，1=高电平空闲                     |
| **CPHA (Clock Phase)**    | 数据采样沿：0=第一个 SCLK 边沿采样数据，1=第二个边沿采样数据 |

- SPI 总共有 4 种模式（Mode0~Mode3）：

  | SPI Mode | CPOL | CPHA | 数据采样沿 |
  | -------- | ---- | ---- | ---------- |
  | Mode0    | 0    | 0    | 上升沿     |
  | Mode1    | 0    | 1    | 下降沿     |
  | Mode2    | 1    | 0    | 下降沿     |
  | Mode3    | 1    | 1    | 上升沿     |

  > 数据输出沿和采样沿的对应关系可以通过手册上的 **SPI Timing Diagram** 确认。

## 1.硬件连接

这里贴一个 SPI 连接原理：[Introduction to SPI Interface](https://www.analog.com/en/resources/analog-dialogue/articles/introduction-to-spi-interface.html)。

基于 SPI 特性，需要检查 RK3568 与 AD9833 的连接线是否正确。理论上的连接线为：

| **AD9833 引脚**   | **说明**                                      | **接 RK3568 SPI**   |
| ----------------- | --------------------------------------------- | ------------------- |
| **MCLK** (Pin 5)  | DDS 工作参考时钟（一般外部晶振，比如 25 MHz） | 独立提供，不走 SPI  |
| **SDATA** (Pin 6) | 串行数据输入（16 位）                         | **MOSI**            |
| **SCLK** (Pin 7)  | 串行时钟输入（下降沿采样）                    | **SCLK**            |
| **FSYNC** (Pin 8) | 帧同步信号（片选，低电平有效）                | **CSN**（SPI 片选） |
| **MISO**          | AD9833 没有数据输出                           | 空着，不用接        |
| **VDD/GND**       | 电源和地                                      | 3.3V / GND          |

- AD9833 在 **SCLK 下降沿采样数据**
- 经过 VOUT 输出数字和模拟信号

> 1️⃣ 硬件前提
>
> - AD9833 工作电压：3.3V
> - 已连接到 RK3568 SPI0 的 **M1 引脚组 / VCCIO5**
> - SPI0_CS0 连接到 AD9833 的 FSYNC
> - SPI0_MOSI → SDATA, SPI0_SCLK → SCLK
> - MISO 不接

### SPI 接口定义

在《RK3568 Hardware Design Guide》的 “2.3.14 SPI 接口电路” 章节中，介绍了关于 RK3568 的 SPI 接口描述。

RK 3568 拥有 4 个通用 SPI 控制器。由于这里我们将 AD9833 挂在了 SPI0 的总线上，所以仅需要看 SPI0 相关的接口定义：

> SPI0 复用 M0/M1，其中 M0: PMUIO2, M1: VCCIO5

意思就是：

- SPI1 有两组可选引脚（同一个 SPI 控制器的不同引脚复用组）：
  - **M0** 引脚组，电平由 **PMUIO2** 决定
  - **M1** 引脚组，电平由 **VCCIO5** 决定
- 你要根据实际外设的供电电压来选择哪组引脚。

```bash
SPI0 控制器
   ├── M0 引脚组 → 属于 PMUIO2 (always-on 域, 电压固定, 常用在低功耗保持)
   └── M1 引脚组 → 属于 VCCIO5 (普通 IO bank, 电压可选 1.8V/3.3V, 适配普通外设)
```

而对应的 GPIO 电源域为：

| 选项                 | 电压 | 优点                   | 缺点                                                   | 适合 AD9833 吗 |
| -------------------- | ---- | ---------------------- | ------------------------------------------------------ | -------------- |
| **SPI0_M0 → PMUIO2** | 3.3V | always-on，低功耗保持  | 电源域是 always-on，可能在低功耗模式下更耗电或占用资源 | ✅ 可以         |
| **SPI0_M1 → VCCIO5** | 3.3V | 普通 IO bank，灵活配置 | 无特别缺点                                             | ✅ 可以         |

**总结**

- **PMUIO2 (M0)**：SPI1 的一套管脚，走在 PMU IO 域 → 特点是掉电保持/always-on，在 RK3568 手册中，固定为3.3V。

- **VCCIO5 (M1)**：SPI1 的另一套管脚，走在 VCCIO5 域 → 电压可选，灵活对接 1.8V / 3.3V 外设。
- 你在 **硬件设计** 时要根据外设供电电压 & 是否需要低功耗保持 来决定选 M0 还是 M1。
- 在 **DTS 软件配置** 里，对应就要用 `spi0_m0` 或 `spi0_m1` 的 pinctrl 配置。

> 在这里，因为我们使用的 AD9833 供电电压`VDD`标准在 +2.3V ~ +5.5V之间，而典型的工作电压在 3.3 V。AD9833 需要 3.3V 的逻辑电平输入，就必须保证 VCCIO 域也供 3.3V，并且 DTS 里 pinctrl 配置成 3.3V 模式。 

## 2.DTS 配置

**Linux** 内核通过 **设备树（Device Tree）** 文件来识别和配置硬件。我们需要修改 **RK3568** 对应的 DTS 文件，来告诉内核连接了一个 **SPI** 设备。

这里我们参阅《Rockchip_Developer_Guide_Linux_SPI_CN》配置自己的 DTS 树。

打开`kernel/arch/arm64/boot/dts/rockchip/rk3568.dtsi`（包含了芯片通用功能的默认配置），`rk3568.dtsi` 文件中已经包含了 **SPI0** 节点：

```C
spi0: spi@fe610000 {
    compatible = "rockchip,rk3066-spi";
    reg = <0x0 0xfe610000 0x0 0x1000>;
    interrupts = <GIC_SPI 103 IRQ_TYPE_LEVEL_HIGH>;
    #address-cells = <1>;
    #size-cells = <0>;
    clocks = <&cru CLK_SPI0>, <&cru PCLK_SPI0>;
    clock-names = "spiclk", "apb_pclk";
    dmas = <&dmac0 20>, <&dmac0 21>;
    dma-names = "tx", "rx";
    pinctrl-names = "default", "high_speed";
    pinctrl-0 = <&spi0m0_cs0 &spi0m0_cs1 &spi0m0_pins>;
    pinctrl-1 = <&spi0m0_cs0 &spi0m0_cs1 &spi0m0_pins_hs>;
    status = "disabled";
};
```

而 MYZR 针对 RK3568 做出了相应的修改，相应开发板的 DTS 文件为`myzr-rk3568-ddr4.dtsi`。其文件中引用了 RK3568 基础文件`rk3568.dtsi`，并对特定板子的硬件进行修改或补充。

如果要将 **SPI0** 的配置从 **`rk3568.dtsi`** 继承的默认值修改为使用 **VCCIO5** 电源域，我们需要在 **`myzr-rk3568-ddr4.dtsi`** 中进行**节点覆盖（node overlay）**。

### 节点覆盖

在 **`myzr-rk3568-ddr4.dtsi`** 中找到或创建一个 **`&spi0`** 节点，然后覆盖其属性。

1. 创建`&spi0`节点

   在文件的任何位置添加以下代码块：

   ```C
   &spi0 {
       // 在这里添加你想要修改的属性
   };
   ```

2. 覆盖默认配置

   目标是启用 SPI0，并将其引脚配置为使用 **VCCIO5** 电源域（对应 `m1` 模式）。

   在 `myzr-rk3568-ddr4.dtsi` 中，添加以下代码来覆盖 `rk3568.dtsi` 中的默认配置：

   ```C
   &spi0 {
       // 启用 SPI0
       status = "okay";
   
       // 覆盖引脚复用，指定使用 VCCIO5 (M1) 引脚组
       pinctrl-names = "default";
       pinctrl-0 = <&spi0_pins_m1>;
       
       // 如果你有高速模式需求，也可以覆盖 pinctrl-1
       // pinctrl-names = "default", "high_speed";
       // pinctrl-1 = <&spi0_pins_m1_hs>;
   };
   ```

   - **`&spi0`**: 这个符号引用了在 `rk3568.dtsi` 中已经定义的 **SPI0** 节点。
   - **`status = "okay";`**: 这会覆盖基准文件中的 `"disabled"` 状态，从而启用 SPI0。
   - **`pinctrl-0 = <&spi0_pins_m1>;`**: 这会覆盖基准文件中的 `pinctrl-0`，使其引用 **VCCIO5** 电源域对应的 `m1` 引脚组。

3. 添加 AD9833 设备

   在同一个 `&spi0` 节点中添加子节点：

   ```C
   &spi0 {
       status = "okay";
       pinctrl-names = "default";
       pinctrl-0 = <&spi0m1_pins_hs &spi0m1_cs0_hs>;
       num-cs = <1>;                                       /* SPI0 支持 CS0 */
   
       spidev@0 {
           compatible = "spidev";
           reg = <0>;                      /* CS0 */
           spi-max-frequency = <25000000>; /* 25 MHz，低于 AD9833 最大 40 MHz */
           spi-cpol;                       // 空闲高或低，根据手册，可选择不加表示空闲低
           spi-cpha;                                               // 数据在 SCLK 第二个边沿采样 → CPHA=1
           status = "okay";
       };
   }
   ```
   
   - `spi-max-frequency `：AD9833 支持的最大 SCLK 频率。在 AD9833 手册中：
   
     >  The serial clock can have a frequency of 40 MHz maximum. The serial clock can be continuous or, it can idle high or low between write operations. 
     
     - “serial clock” = SCLK
     
     - “can have a frequency of 40 MHz maximum” → **最大 SCLK = 40 MHz**
     
      AD9833 支持的最高 SPI 时钟频率为 40 Mhz。在 DTS 中 `spi-max-frequency` 建议略低于 40 MHz，例如 30 MHz 或 25 MHz，保证可靠。
     
   - `spi-cpol/spi-cpha`：SPI 通信的 **时钟极性（CPOL）** 和 **时钟相位（CPHA）**
   
     - “Data is clocked into the AD9833 on each falling SCLK edge” → 数据在 **SCLK 下降沿**采样
     - “The serial clock can idle high or low” → CPOL 可选（空闲高/低都可以）
     - 结合 SPI 模式表：
   
     | 空闲电平 | 下降沿采样 | 推荐 SPI Mode          |
     | -------- | ---------- | ---------------------- |
     | 空闲低   | 下降沿     | Mode1 (CPOL=0, CPHA=1) |
     | 空闲高   | 下降沿     | Mode2 (CPOL=1, CPHA=0) |
   

## 3.验证

配置完毕后，打包为镜像启动，检查 DTS 配置的 SPI 是否生效。

### 1.检查 SPI 总线是否已启用

首先，需要确认 SPI 控制器本身是否已成功启用。您可以通过登录到开发板的 Linux 系统，查看 `/sys/bus/spi/devices/` 目录。

```bash
ls /sys/bus/spi/devices/
```

如果设备树配置正确，应该能看到一个名为 `spi0.0` 的目录。这个目录名表示：

- **`spi0`**: SPI 控制器编号为0。
- **`.0`**: 连接在该控制器上的设备，使用了片选线0。

这个目录的存在表明内核已经识别并成功初始化了 SPI 控制器，并且找到了 SPI 从设备。

这里结果为：

```bash
root@RK356X:/# ls /sys/bus/spi/devices/
spi0.0	spi3.0
```

### 2.检查 `spidev` 设备节点是否生成

在设备树中使用了 `compatible = "spidev";`，那么内核会为您的 SPI 设备创建一个用户空间接口。

可以通过以下命令检查 `/dev` 目录下是否生成了对应的设备文件：

```bash
root@RK356X:/# ls /dev/spidev* 
/dev/spidev0.0	/dev/spidev3.0
```

如果成功，我们应该能看到一个名为 `/dev/spidev0.0` 的设备文件。

- **`spidev`**: 表示这是一个 `spidev` 设备。
- **`0`**: 对应 SPI 总线编号0。
- **`.0`**: 对应片选线编号0。

这个文件的存在意味着您可以在用户空间编写程序来直接读写这个文件，从而与 AD9833 芯片通信。

### 3.检查内核日志

在系统启动过程中，SPI 驱动会打印一些初始化信息。我们可以通过查看内核日志来验证驱动是否加载成功以及是否有错误。

```bash
root@RK356X:/# dmesg | grep spi
[    1.799691] rockchip-spi fe610000.spi: no high_speed pinctrl state
[    1.800166] /spi@fe610000/spidev@0: buggy DT: spidev listed directly in DT
```

1. 警告：`buggy DT: spidev listed directly in DT`

   `spidev` 是一个通用的用户空间驱动，它没有与特定硬件（如AD9833）相关联。按照设备树的最佳实践，设备树应该描述**硬件**，而不是驱动程序。

   虽然是警告，但功能可能正常

2. 错误：`no high_speed pinctrl state`

   SPI 控制器驱动尝试切换到 `high_speed` pinctrl 状态，但是在 DTS 中找不到

   - 原因：

     - 我们配置 DTS 使用了 `&spi0m1_pins_hs` 和 `&spi0m1_cs0_hs`，这是 **high_speed 引脚组**
     - 但是 **rockchip-spi 驱动期望 pinctrl state 名字为 "high-speed"**，并且 SPI 控制器节点必须定义 `pinctrl-0 = <&pinctrl_default>;` + `pinctrl-names = "default";`
     - 直接把 HS 引脚绑定到 spidev 节点，会触发驱动检查失败

   - 解决方案：

     ```bash
     &spi0 {
         status = "okay";
         pinctrl-names = "default", "high_speed";
         pinctrl-0 = <&spi0m1_pins &spi0m1_cs0>;
         pinctrl-1 = <&spi0m1_pins_hs &spi0m1_cs0_hs>;
         num-cs = <1>;                                       /* SPI0 支持 CS0 */
     
         spidev@0 {
             compatible = "spidev";
             reg = <0>;                      /* CS0 */
             spi-max-frequency = <25000000>; /* 25 MHz，低于 AD9833 最大 40 MHz */
             spi-cpol;                       // 空闲高
             spi-cpha;                       // 数据在 SCLK 第二个边沿采样 → CPHA=1
             status = "okay";
         };
     };
     ```
     

这样再次编译后，就能通过了。

## 4.进行实际通信测试

如果前面的检查都通过了，说明内核层面的配置是正确的。但要真正确认 AD9833 芯片能被控制，我们必须进行实际的 SPI 数据读写测试。

我的想法是，编写一个简单的用户层程序。这个程序会通过 **`/dev/spidev0.0`** 这个设备节点，向 **AD9833** 发送一组特定的控制字。如果通信成功，你可以用示波器观察 **AD9833** 的输出引脚，看是否有波形产生。
