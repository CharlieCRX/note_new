# MAC-TO-MAC



现在需要让 RK3568 的 GMAC1（`ethernet@fe010000`）以 MAC to MAC 的方式，与 FPGA 通信。

## DTS设备树

当前 gmac1 的核心配置(`rk3568.dtsi`)为：

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

对应特定开发板配置为：

```C
&gmac1 {
        phy-mode = "rgmii";
        clock_in_out = "output";

        assigned-clocks = <&cru SCLK_GMAC1_RX_TX>, <&cru SCLK_GMAC1>;
        assigned-clock-parents = <&cru SCLK_GMAC1_RGMII_SPEED>;
        assigned-clock-rates = <0>, <125000000>;

        pinctrl-names = "default";
        pinctrl-0 = <&gmac1m1_miim
                &gmac1m1_tx_bus2
                &gmac1m1_rx_bus2
                &gmac1m1_rgmii_clk
                &gmac1m1_rgmii_bus
                                &gmac1m1_clkinout>;

        tx_delay = <0x20>;
        rx_delay = <0x20>;

        status = "okay";

        fixed-link {
                 speed = <1000>;
                 full-duplex;
        };

};

```

