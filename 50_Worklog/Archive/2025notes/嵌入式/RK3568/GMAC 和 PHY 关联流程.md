本节将从启动日志`dmesg`入手，介绍`RK3568`芯片启动的时候，eth、GMAC 和 PHY 是如何绑定关联的。

本节将会用到**用 DTS 配置参数和内核打印进行交叉验证**的方法进行探究。

## eth 绑定 GMAC

### 日志记录

输入命令`dmesg | grep -i eth`，看下日志中关于网口和 MAC 之间的绑定关系：

```bash
[   10.839567] rk_gmac-dwmac fe010000.ethernet eth0: stmmac_hw_setup: DMA engine initialization failed
[   10.839576] rk_gmac-dwmac fe010000.ethernet eth0: stmmac_open: Hw setup failed
[   10.943828] rk_gmac-dwmac fe2a0000.ethernet: Failed to reset the dma
[   10.943862] rk_gmac-dwmac fe2a0000.ethernet eth1: stmmac_hw_setup: DMA engine initialization failed
[   10.943870] rk_gmac-dwmac fe2a0000.ethernet eth1: stmmac_open: Hw setup failed
```

可以看出：

- `eth0`绑定了地址为`fe010000`的GMAC 控制器
- `eth1`绑定了地址为`fe2a0000`的GMAC 控制器

那么对应这两个 GMAC 寄存器的地址，其具体定义在对应的 DTS 文件中。这里针对当前开发环境的`RK3568`芯片，其文件在

```bash
kernel/arch/arm64/boot/dts/rockchip/rk3568.dtsi
```

我们打开 DTS 看下对于这两个 GMAC 控制器的详细配置。

### DTS 文件

我们打开`rk3568.dtsi`，找到这两个 gmac 的基本定义：

首先是`fe010000`的定义：

```C
gmac1: ethernet@fe010000 {
        compatible = "rockchip,rk3568-gmac", "snps,dwmac-4.20a";
        reg = <0x0 0xfe010000 0x0 0x10000>;
        interrupts = <GIC_SPI 32 IRQ_TYPE_LEVEL_HIGH>,
                     <GIC_SPI 29 IRQ_TYPE_LEVEL_HIGH>;
        interrupt-names = "macirq", "eth_wake_irq";
        rockchip,grf = <&grf>;
        clocks = <&cru SCLK_GMAC1>, <&cru SCLK_GMAC1_RX_TX>,
                 <&cru SCLK_GMAC1_RX_TX>, <&cru CLK_MAC1_REFOUT>,
                 <&cru ACLK_GMAC1>, <&cru PCLK_GMAC1>,
                 <&cru SCLK_GMAC1_RX_TX>, <&cru CLK_GMAC1_PTP_REF>,
                 <&cru PCLK_XPCS>;
        clock-names = "stmmaceth", "mac_clk_rx",
                      "mac_clk_tx", "clk_mac_refout",
                      "aclk_mac", "pclk_mac",
                      "clk_mac_speed", "ptp_ref",
                      "pclk_xpcs";
        resets = <&cru SRST_A_GMAC1>;
        reset-names = "stmmaceth";

        snps,mixed-burst;
        snps,tso;

        snps,axi-config = <&gmac1_stmmac_axi_setup>;
        snps,mtl-rx-config = <&gmac1_mtl_rx_setup>;
        snps,mtl-tx-config = <&gmac1_mtl_tx_setup>;
        status = "disabled";

        mdio1: mdio {
                compatible = "snps,dwmac-mdio";
                #address-cells = <0x1>;
                #size-cells = <0x0>;
        };

        gmac1_stmmac_axi_setup: stmmac-axi-config {
                snps,wr_osr_lmt = <4>;
                snps,rd_osr_lmt = <8>;
                snps,blen = <0 0 0 0 16 8 4>;
        };

        gmac1_mtl_rx_setup: rx-queues-config {
                snps,rx-queues-to-use = <1>;
                queue0 {};
        };

        gmac1_mtl_tx_setup: tx-queues-config {
                snps,tx-queues-to-use = <1>;
                queue0 {};
        };
};
```

另一个就是`fe2a0000`的定义：

```C
gmac0: ethernet@fe2a0000 {
        compatible = "rockchip,rk3568-gmac", "snps,dwmac-4.20a";
        reg = <0x0 0xfe2a0000 0x0 0x10000>;
        interrupts = <GIC_SPI 27 IRQ_TYPE_LEVEL_HIGH>,
                     <GIC_SPI 24 IRQ_TYPE_LEVEL_HIGH>;
        interrupt-names = "macirq", "eth_wake_irq";
        rockchip,grf = <&grf>;
        clocks = <&cru SCLK_GMAC0>, <&cru SCLK_GMAC0_RX_TX>,
                 <&cru SCLK_GMAC0_RX_TX>, <&cru CLK_MAC0_REFOUT>,
                 <&cru ACLK_GMAC0>, <&cru PCLK_GMAC0>,
                 <&cru SCLK_GMAC0_RX_TX>, <&cru CLK_GMAC0_PTP_REF>,
                 <&cru PCLK_XPCS>;
        clock-names = "stmmaceth", "mac_clk_rx",
                      "mac_clk_tx", "clk_mac_refout",
                      "aclk_mac", "pclk_mac",
                      "clk_mac_speed", "ptp_ref",
                      "pclk_xpcs";
        resets = <&cru SRST_A_GMAC0>;
        reset-names = "stmmaceth";

        snps,mixed-burst;
        snps,tso;

        snps,axi-config = <&gmac0_stmmac_axi_setup>;
        snps,mtl-rx-config = <&gmac0_mtl_rx_setup>;
        snps,mtl-tx-config = <&gmac0_mtl_tx_setup>;
        status = "disabled";

        mdio0: mdio {
                compatible = "snps,dwmac-mdio";
                #address-cells = <0x1>;
                #size-cells = <0x0>;
        };

        gmac0_stmmac_axi_setup: stmmac-axi-config {
                snps,wr_osr_lmt = <4>;
                snps,rd_osr_lmt = <8>;
                snps,blen = <0 0 0 0 16 8 4>;
        };

        gmac0_mtl_rx_setup: rx-queues-config {
                snps,rx-queues-to-use = <1>;
                queue0 {};
        };

        gmac0_mtl_tx_setup: tx-queues-config {
                snps,tx-queues-to-use = <1>;
                queue0 {};
        };
};
```

- 内核日志中显示`eth0`对应的设备是`fe010000.ethernet`， **eth1** 对应的设备是 `fe2a0000.ethernet`。
- 而在设备树`rk3568.dtsi`中，`fe010000`是 **gmac1 的寄存器基地址**；`fe2a0000` 是 **gmac0 的寄存器基地址**。
- 这说明 **eth1 绑定的 MAC 控制器就是 gmac0**；同理 **eth0 绑定的 MAC 控制器就是 gmac1**

并且日志中还存在两个GMAC对应的 RGMII TX/RX 时钟延迟配置，这可以帮我们进一步验证：

```bash
[    1.814187] rk_gmac-dwmac fe010000.ethernet: TX delay(0x4f).
[    1.814201] rk_gmac-dwmac fe010000.ethernet: RX delay(0x26).

[    1.949217] rk_gmac-dwmac fe2a0000.ethernet: TX delay(0x3c).
[    1.949231] rk_gmac-dwmac fe2a0000.ethernet: RX delay(0x2f).
```

也就是说：

- GMAC控制器`fe010000`的`RGMII`的 TX 延迟为 `0x4f`，RX 延迟为`0x26`
- GMAC控制器`fe2a0000`的`RGMII`的 TX 延迟为`0x3c`， RX 延迟为`0x2f`

这是 **Rockchip 平台 GMAC 驱动 (stmmac 驱动 + Rockchip glue 层)** 在 probe 时，打印出的 **RGMII TX/RX 时钟延迟配置**。

这些 delay 值不是驱动随便算的，而是来自 **设备树 (DTS)**。

这里的 DTS 配置在`kernel/arch/arm64/boot/dts/rockchip/myzr-rk3568-ddr4.dtsi`。

打开`myzr-rk3568-ddr4.dtsi`，查看对应gmac的板级特定配置：

gmac0：

```C
&gmac0 {
        phy-mode = "rgmii";
        clock_in_out = "input";
        snps,reset-gpio = <&gpio3 RK_PB2 GPIO_ACTIVE_LOW>;
        snps,reset-active-low;
        /* Reset time is 20ms, 100ms for rtl8211f */
        snps,reset-delays-us = <0 20000 100000>;

        assigned-clocks = <&cru SCLK_GMAC0_RX_TX>, <&cru SCLK_GMAC0>;
        assigned-clock-parents = <&cru SCLK_GMAC0_RGMII_SPEED>, <&gmac0_clkin>;
        assigned-clock-rates = <0>, <125000000>;

        pinctrl-names = "default";
    pinctrl-0 = <&gmac0_miim
                &gmac0_tx_bus2
                &gmac0_rx_bus2
                &gmac0_rgmii_clk
                &gmac0_rgmii_bus
                                &gmac0_clkinout>;


        tx_delay = <0x3c>;
        rx_delay = <0x2f>;

        phy-supply = <&vcc3v3_phy>;
        phy-handle = <&rgmii_phy0>;
        status = "okay";
};

```

gmac1：

```C
&gmac1 {
        phy-mode = "rgmii";
        clock_in_out = "input";
        snps,reset-gpio = <&gpio3 RK_PB0 GPIO_ACTIVE_LOW>;
        snps,reset-active-low;
        /* Reset time is 20ms, 100ms for rtl8211f */
        snps,reset-delays-us = <0 20000 100000>;

        assigned-clocks = <&cru SCLK_GMAC1_RX_TX>, <&cru SCLK_GMAC1>;
        assigned-clock-parents = <&cru SCLK_GMAC1_RGMII_SPEED>, <&gmac1_clkin>;
        assigned-clock-rates = <0>, <125000000>;

        pinctrl-names = "default";
        pinctrl-0 = <&gmac1m1_miim
                &gmac1m1_tx_bus2
                &gmac1m1_rx_bus2
                &gmac1m1_rgmii_clk
                &gmac1m1_rgmii_bus
                                &gmac1m1_clkinout>;

        tx_delay = <0x4f>;
        rx_delay = <0x26>;

        phy-handle = <&rgmii_phy1>;
        status = "okay";
};
```

可以看出在 DTS 设备中，配置的 GMAC 参数为：

- `gmac0`：TX = 0x3c，RX = 0x2f
- `gmac1`：TX = 0x4f，RX = 0x26

而上面的

> - GMAC控制器`fe010000`的`RGMII`的 TX 延迟为 `0x4f`，RX 延迟为`0x26`
> - GMAC控制器`fe2a0000`的`RGMII`的 TX 延迟为`0x3c`， RX 延迟为`0x2f`

正好对应了：

- GMAC 地址 = `fe010000` -> gmac1
- GMAC 地址 = `fe2a0000` -> gmac0

> 这里其实多此一举了。因为上面 rk3568.dtsi 声明的别名 gmac0 和 gmac1 就已经关联到对应的 GMAC 控制器了。

## GMAC 绑定 PHY

上面通过日志 + rk3568.dtsi 已经获取到了 eth 绑定的 GMAC 控制器关系。RK3568的

下一步就是需要理解 GMAC 如何访问到 PHY的。

首先我们以`phy`为关键词，搜索内核中关于网络 PHY 芯片的相关日志：

```bash
dmesg | grep -i phy
```

输出为：

```bash
root@RK356X:/# dmesg | grep -i phy
[    0.000000] Booting Linux on physical CPU 0x0000000000 [0x412fd050]
...
[    1.814111] rk_gmac-dwmac fe010000.ethernet: Looking up phy-supply from device tree
[    1.814127] rk_gmac-dwmac fe010000.ethernet: Looking up phy-supply property in node /ethernet@fe010000 failed
[    1.814219] rk_gmac-dwmac fe010000.ethernet: integrated PHY? (no).
[    1.814264] rk_gmac-dwmac fe010000.ethernet: clock input from PHY
[    1.949133] rk_gmac-dwmac fe2a0000.ethernet: Looking up phy-supply from device tree
[    1.949149] rk_gmac-dwmac fe2a0000.ethernet: Looking up phy-supply property in node /ethernet@fe2a0000 failed
[    1.949249] rk_gmac-dwmac fe2a0000.ethernet: integrated PHY? (no).
[    1.949296] rk_gmac-dwmac fe2a0000.ethernet: clock input from PHY
...
[   10.737352] Generic PHY stmmac-1:02: attached PHY driver [Generic PHY] (mii_bus:phy_addr=stmmac-1:02, irq=POLL)
[   10.842452] Generic PHY stmmac-0:01: attached PHY driver [Generic PHY] (mii_bus:phy_addr=stmmac-0:01, irq=POLL)
```

其关键信息为：

### （A）加载 GMAC 控制器驱动

```bash
[    1.814***] rk_gmac-dwmac fe010000.ethernet: ...
[    1.949***] rk_gmac-dwmac fe2a0000.ethernet: ...
```

stmmac 驱动分配名称的顺序是基于设备被 Linux 内核探测到的顺序，而不是设备树（DTS）中定义的顺序。

stmmac  驱动，每实例化一个 GMAC，就会分配一个唯一的名字：`stmmac-0`、`stmmac-1`、`stmmac-2` ……(这里编号可能是跟MAC 编号一致，像 MAC0 对应 stmmac-0)

- 第一个探测到的 GMAC（这里是 `fe010000.ethernet`） → `stmmac-0`
- 第二个探测到的 GMAC（这里是 `fe2a0000.ethernet`） → `stmmac-1`

（顺序由设备树和总线扫描顺序决定，一般和 DTS 节点声明顺序一致）

说明 DTS 里的 **ethernet@fe010000** （gmac1）和 **ethernet@fe2a0000**（gmac0） 两个 GMAC 节点被驱动`rk_gmac-dwmac`探测到了。

而当 GMAC 初始化时：

1. `dwmac-mdio`驱动会创建一个 `mii_bus`，名字就是 `stmmac-N`。
2. 通过 `phy-handle`（DTS 属性）去找对应的 MDIO 总线和 PHY 节点。
3. 绑定成功后，你就会看到10s后的日志。

### （B）扫描 MDIO 总线 → 绑定 PHY 驱动

过了 10 秒左右，系统网络子系统初始化：

```bash
[   10.737352] Generic PHY stmmac-1:02: attached PHY driver [Generic PHY] (mii_bus:phy_addr=stmmac-1:02, irq=POLL)
[   10.842452] Generic PHY stmmac-0:01: attached PHY driver [Generic PHY] (mii_bus:phy_addr=stmmac-0:01, irq=POLL)
```

- `stmmac-1:02` → 表示 PHY 在 `stmmac-1` 这条 RGMII总线上的地址 0x02。
- `stmmac-0:01` → 表示 PHY 在 `stmmac-0` 这条 RGMII 总线上的地址 0x01。

这说明了：

- GMAC 设备`fe010000.ethernet`（对应`gmac1`），其对应的 PHY 在 REGII 总线上的地址为`0x02`
- GMAC 设备`fe2a0000.ethernet`（对应`gmac0`），其对应的 PHY 在 REGII 总线上的地址为`0x01`

到此为止，就分析完毕eth - MAC - PHY 的关系了。

## Linux 内核初始化 **MAC + PHY 的流程**

### 1️⃣ MAC 驱动 probe 阶段

- 以 RK356X 为例，GMAC 控制器对应的驱动是 `stmmac`。

- 内核启动时，会扫描设备树（Device Tree）里的 GMAC 节点，例如：

  ```C
  &gmac0 {
      status = "okay";
      phy-handle = <&phy0>;
  };
  ```

- 驱动的 `probe` 函数会被调用

  - 申请和映射 MAC 的寄存器。
  - 初始化 DMA、描述符等。
  - 通过 `of_get_phy` 或 `mdio` API 找到挂在这个 MAC 上的 PHY。

### 2️⃣ MDIO 总线扫描 PHY

- MAC 驱动初始化完成后，会调用 `mdio_scan`：
  - 内核扫描 PHY 地址（通常是 0~31）上的所有设备。
  - 对每个找到的 PHY，创建对应的 `mdio_device` 结构。
  - 每个 `mdio_device` 在 sysfs 里就会出现一个目录 `/sys/bus/mdio_bus/devices/<bus>:<addr>`，比如 `stmmac-0:00`：
    - `stmmac-0` → 第 0 个 stmmac 控制器（MAC0）
    - `:00` → PHY 地址 0

### 3️⃣ PHY 驱动 probe 阶段

- 当 MDIO 总线发现 PHY 后，会根据 `compatible` 匹配 PHY 驱动。例如

  ```c
  &mdio0 {
      rgmii_phy0: phy@0 {
          compatible = "ethernet-phy-ieee802.3-c22";
          reg = <0>;
      };
  };
  ```

- PHY 驱动的 `probe` 会被调用，完成：

  - PHY 寄存器初始化（如 Auto-Negotiation、速度/双工配置）。
  - 将 PHY 状态注册到 `net_device` 的 `phydev` 中。
  - sysfs 下的 `stmmac-0:00` 目录也会被创建，用于用户空间访问 PHY。

**Linux 内核在 MAC 初始化并扫描 PHY 的过程中，PHY 被识别并注册到 MDIO 总线后，sysfs 节点就出现了**。
