测试目标：确认开发板 `UART2` 对应的 RS485 接口可以正常发送、接收数据。

硬件连接：

```
开发板 RS485_A  <-> USB-RS485 A
开发板 RS485_B  <-> USB-RS485 B
开发板 GND      <-> USB-RS485 GND，建议连接
```

如果无数据或乱码，可以交换 A/B 再测试。

串口参数统一设置为：

```
波特率：115200
数据位：8
校验位：None
停止位：1
流控：None
```

开发板串口节点：

```
/dev/ttyS2
```

先在开发板配置 UART2：

```
stty -F /dev/ttyS2 115200 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw -echo
```

**1. 开发板发送，PC 接收**

开发板执行：

```
while true; do
    printf 'UUUUUUUU\r\n' > /dev/ttyS2
    sleep 1
done
```

PC 串口助手选择对应 USB-RS485 串口，打开接收。  
如果选择 HEX 显示，应看到：

```
55 55 55 55 55 55 55 55 0D 0A
```

如果选择 ASCII 显示，应看到：

```
UUUUUUUU
```

判定标准：PC 能稳定收到上述数据，说明 `UART2 -> RS485 -> USB-RS485 -> PC` 方向正常。

**2. PC 发送，开发板接收**

开发板执行：

```
cat /dev/ttyS2
```

或者如果系统支持 `hexdump`：

```
hexdump -Cv /dev/ttyS2
```

PC 串口助手发送 ASCII：

```
pc-to-board-test
```

如果开发板终端能看到对应字符串，说明 `PC -> USB-RS485 -> RS485 -> UART2` 方向正常。

也可以 PC 发送 HEX：

```
55 55 55 55 0D 0A
```

开发板 `hexdump` 应能看到类似：

```
55 55 55 55 0d 0a
```

**3. 长时间稳定性测试**

开发板周期发送带编号的数据：

```
i=0
while true; do
    printf 'uart2-rs485-test-%06d\r\n' "$i" > /dev/ttyS2
    i=$((i + 1))
    sleep 0.2
done
```

PC 端观察是否连续、无乱码、无明显丢包。

判定标准：

```
连续接收 5~10 分钟无乱码
编号基本连续
无异常断开
```

**4. 异常判断**

如果 `CPU_UART2_TX` 有波形，但 PC 收不到：

```
检查 RS485 DE/RE 方向控制
检查 A/B 是否接反
检查 USB-RS485 串口号是否正确
检查两端波特率是否一致
检查 GND 是否共地
```

如果 PC 能收到，但有乱码：

```
优先检查波特率、校验位、停止位
再检查 A/B 接线、线缆长度、终端电阻、共地
```

最终结论写法可以是：

```
开发板 UART2 映射为 /dev/ttyS2。通过 USB-RS485 与 PC 串口助手进行双向通信测试，开发板发送数据 PC 可正常接收，PC 发送数据开发板可正常接收，说明 UART2 RS485 接口通信功能正常。
```