Windows 端监听并保存 10 秒：

```
python capture_and_plot.py capture --port 5030 --seconds 10 --output ak5538_test --source-ip 10.1.2.161
```

会生成：

```
ak5538_test.packets   # 原始 UDP 包，后续可反复解析
ak5538_test.txt       # 包数、来源、包长、到达顺序统计
```

检查传输序号是否连续：

```
python capture_and_plot.py analyze --output ak5538_test
```

你希望看到类似：

```
missing_if_sequence_is_consecutive=0
common_positive_sequence_deltas=[(1, ...)]
```

按当前脚本的固定假设解析绘图（8 路、32 位整数）：

```
python capture_and_plot.py plot --output ak5538_test --channels 8 --endian little --points 20000
```

也应测试大端版本：

```
python capture_and_plot.py plot --output ak5538_test --channels 8 --endian big --points 20000
```

若已替换为新版脚本，还可以测试 16 位格式：

```
python capture_and_plot.py plot --output ak5538_test --channels 16 --sample-width 16 --endian little --points 20000
```

开发板侧启动发送：

```
collect_demo -i 10.1.2.111 -p 5030 -t 10
```

其中 `10.1.2.111` 是 Windows IP，`5030` 必须与 Python `--port` 相同。

需要与 FPGA 工程师确认的内容如下。这些信息决定 Python 的绘图参数，也决定“FPGA 发出的包”和“PC 转发的包”是否是同一种格式。

| 需要确认项       | 必须明确的内容                                               | 对脚本的影响                           |
| ---------------- | ------------------------------------------------------------ | -------------------------------------- |
| UDP 负载结构     | 前 4 字节是否为网络序递增包号；之后是否刚好是 ADC 原始数据   | `capture/analyze` 的序号解析前提       |
| 每包长度         | FPGA UDP payload 是否固定 1028 字节，即 4 字节序号 + 1024 字节数据 | 当前脚本按此理解                       |
| 采样位宽         | 每通道样本是 16、24 还是 32 bit；24 位是否装在 32-bit slot   | `--sample-width`；24 位需增加专门解码  |
| 字节序           | 一个采样字在 UDP 中是小端还是大端                            | `--endian little/big`                  |
| 通道数           | 每个采样时刻包含几路 ADC 数据：2、8、16 或其他               | `--channels N`                         |
| 通道排列         | 数据是否为 `CH0, CH1, ... CH7` 循环交织；还是 TDM 分组、双声道串行口拼接、各通道分块 | 决定是否能直接 `reshape(-1, channels)` |
| AK5538 串行输出  | 使用哪些 SDTO 引脚；每个引脚承载哪些通道；Normal I2S 还是 TDM；LRCK/BCLK 的关系 | 判断 FPGA 的解帧是否与 AK5538 配置一致 |
| 位对齐           | Normal I2S 的一个 bit 延迟是否在 FPGA 中正确处理；有效位从 slot 的哪一位开始 | 错一位就会造成类似随机波形             |
| 有符号格式       | 二补码有符号、偏移二进制，还是其他编码                       | 当前脚本假设有符号整数                 |
| 缩放规则         | 原始码值对应的满量程电压、是否左对齐/右对齐、有无截断        | 用于把码值换算成 V                     |
| 包边界           | 一个 UDP 包是否总从完整采样帧开始；包尾是否刚好结束在完整帧  | 若不是，脚本不能逐包直接拼接/重排      |
| 包号含义         | 包号是“每个 UDP 包 +1”，还是“每个采样帧累计值”               | `analyze` 对缺包的判定                 |
| FPGA 发包端口/IP | FPGA 到 `eth1` 的源 MAC/IP/端口、PC 转发时是否保留原始负载   | 排查是否混入了其他 FPGA 数据流         |