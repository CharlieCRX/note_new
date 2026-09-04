- `PAD_Name` = 这个房间的门牌号
- `SW_Name` = 软件系统里登记的房间编号
- 后面的 `UART / SPI / JTAG / GPIO` = 这个房间当前被用来做什么
真正决定这个管脚“现在是 UART 还是 GPIO 还是 JTAG”的，不是 `PAD_Name` 或 `SW_Name` 本身，而是 **pinmux / function select 配置**。