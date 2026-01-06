承接之前[[开发板网络连接问题]]，现在定位到了 DTS。这个章节，就讲解下`RK3568`的dts的几个问题：
- dts 定义及其作用
- `RK3568` 中dts 与网络相关的内容是什么

## 1. 什么是 DTS

DTS（Device Tree Source）本质上是一个**硬件描述文件**，用文本形式把**硬件连接和属性**记录下来，让 Linux 内核在启动时知道板子上有什么外设、它们是怎么连的、用什么参数工作。

你可以把它想象成：

> **硬件说明书 + 接线图 + 配置表**
> 用来告诉内核：“我这块板子长什么样、接口在哪、要怎么用”。

## 2. 它是怎么工作的

- DTS 是**人类可读**的文本文件
- 编译成 **DTB**（Device Tree Blob，二进制）
- U-Boot 或引导程序把 DTB 传给内核
- 内核解析 DTB，按里面的描述加载驱动、初始化外设

流程：

```bash
DTS(文本) --[dtc编译]--> DTB(二进制) --> 内核启动时读取
```

## 3. RK3568 的 DTS

本 DTS 的数据存放在：

```bash
./kernel/arch/arm64/boot/dts/rockchip/myzr-rk3568-ddr4.dtsi
```

### 1.MDIO接口配置

在设备树中，`mdio0` 和 `mdio1` 是 **MDIO 总线接口的标识符**，它们通常映射到硬件平台上的两个不同的 **MDIO 总线控制器**。每个 MDIO 总线控制器可以管理一组 PHY 芯片。

```C
&mdio0 {
    rgmii_phy0: phy@0 {
        compatible = "ethernet-phy-ieee802.3-c22";
        reg = <0>;
    };
};

&mdio1 {
    rgmii_phy1: phy@2 {
        compatible = "ethernet-phy-ieee802.3-c22";
        reg = <2>;
    };
};
```

- **`mdio0`**：表示第一个 MDIO 总线，控制与 **MDIO 总线 0** 相关的 **PHY 芯片**。它可以连接多个 PHY 设备。
- **`mdio1`**：表示第二个 MDIO 总线，控制与 **MDIO 总线 1** 相关的 **PHY 芯片**。它可以连接其他的 PHY 设备。

这些总线通常由 **网络控制器** 或 **SoC** 上的 **MAC 控制器** 提供支持。在设备树中，`&mdio0` 和 `&mdio1` 作为总线的引用，指向具体的硬件控制器或驱动。

- **`phy@0`**：是设备树中节点的名字，表示该节点是一个 **PHY 芯片**，并且在设备树中命名为 `phy@0`

- **`reg = <0>`**：是表示该 PHY 芯片在 **MDIO 总线** 上的 **唯一地址**。即 **`reg = <0>`** 表示 PHY 芯片的地址是 0。

  > `phy@0` 是设备树中的节点名称，而 `reg = <0>` 则指定了该 PHY 芯片在 MDIO 总线上的地址。也就是说，理论上设备树中的节点名称和实际的 **PHY 地址** 是可以不同的。

而**`rgmii_phy0`** 是设备树中的别名，用于引用 **`phy@0`** 节点。它只是一个标签，让设备树的结构更加清晰和易于维护。

所以这段 MDIO 接口配置，最终决定了：

- `mdio0`总线上，绑定了地址为 0 的 PHY 芯片，且设备树名为`phy@0`，别名为`rgmii_phy0`
- `mdio1`总线上，绑定了地址为 2 的 PHY 芯片，且设备树名为`phy@2`，别名为`rgmii_phy1`

### 2. GMAC控制器配置

GMAC 名为 `以太网 MAC`。GMAC 的控制器负责实际的数据包处理和转发，并且与PHY芯片通过**RGMII**接口进行通信。

```C
&gmac0 {
    phy-mode = "rgmii";
    clock_in_out = "input";
    snps,reset-gpio = <&gpio3 RK_PB2 GPIO_ACTIVE_LOW>;
    snps,reset-active-low;
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

- `phy-mode = "rgmii"`：指定使用**RGMII**接口与PHY进行数据交换。

- `phy-handle = <&rgmii_phy0>`：指定此GMAC控制器连接的PHY设备（这里是`rgmii_phy0`，即`phy@0`）。

- `tx_delay` 和 `rx_delay`：设置RGMII信号的时序延迟，用于确保数据传输的同步性，避免信号冲突或错误。

### 3.总结

1. MDIO 配置：用于连接 MAC 和  PHY 之间的管理信号
2. GMAC控制器配置：对应每个 MDIO 总线控制器，配置了单独的以太网 MAC（GMAC） 控制器
3. 硬件连接
   - MDIO 总线：用于配置和管理 PHY 设备，如读取链路状态或调整 PHY 设置。
   - RGMII 总线：用于数据传输，将数据从MAC层传递到PHY层，进行实际的以太网数据交换。

## 4.验证 DTS 配置

