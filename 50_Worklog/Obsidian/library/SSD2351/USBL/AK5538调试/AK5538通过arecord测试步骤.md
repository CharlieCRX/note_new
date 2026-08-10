可以按四个层次验证：配置正确、时钟正确、DMA确实收数、采样内容正确。仅仅 `arecord` 不报错和文件非空，只能证明 DMA 在运行，不能证明位对齐和通道映射正确。

## 1. 测试前确认内核状态

```
test ! -e /sys/bus/i2c/devices/1-0010 \
    && echo "AK5538 userspace I2C available" \
    || echo "FAIL: AK5538 still owned by kernel"

cat /proc/asound/pcm
arecord -l
ls -l /dev/snd/pcmC0D1c
```

预期包含：

```
00-01: ... sstar-dummy-codec2-1 : : capture 1
/dev/snd/pcmC0D1c
```

再确认地址 `0x10` 可由用户态访问：

```
i2cget -y 1 0x10 0x00
```

不能使用 `i2cget -f` 或 `i2cset -f`。

## 2. 执行初始化脚本

将脚本放到开发板，例如：

```
chmod +x /mnt/ak5538_userspace_init.sh
/mnt/ak5538_userspace_init.sh
```

也可以直接：

```
sh /mnt/ak5538_userspace_init.sh
```

脚本应全部显示 `[OK]`，关键回读值应为：

```
POWER_MGMT1  0x00 = 0xff
POWER_MGMT2  0x01 = 0x01
CONTROL1     0x02 = 0x07
CONTROL2     0x03 = 0x00
CONTROL3     0x04 = 0x00
```

可以再手动核对：

```
for reg in 0x00 0x01 0x02 0x03 0x04; do
    echo -n "$reg = "
    i2cget -y 1 0x10 "$reg"
done

fpga -r 0x2
fpga -r 0x5
```

预期：

```
FPGA 0x2[3:0] = 7       # 24.576 MHz
FPGA 0x5 bit2 = 1       # AK5538 外部复位已释放
```

当前脚本见：[ak5538_userspace_init.sh (line 1)](/home/crx/work/SDSONAR/docs/ak5538/ak5538_userspace_init.sh:1)。

## 3. 示波器验证物理时钟

初始化脚本执行完成后，即使没有运行 `arecord`，AK5538 仍可能持续输出 BCLK、LRCK 和 SDTO，这是正常的；`arecord` 只控制 CPU DMA 是否接收。

192 kHz、双通道、32-bit I²S 的预期值：

```
MCLK ≈ 24.576 MHz
LRCK ≈ 192 kHz
BCLK ≈ 12.288 MHz
BCLK/LRCK = 64
MCLK/LRCK = 128
```

应同时检查：

- AK5538 的 SDTO 接到了 CPU 对应 SDI；
- BCLK、LRCK 没有停顿或明显毛刺；
- AK5538 和 CPU 的 I²S 数据采样边沿一致；
- LRCK 翻转后存在标准 I²S 的一位延迟；
- 没有两个器件同时驱动 BCLK/LRCK。

如果这一步不正确，不要继续用录音文件判断 CPU。

## 4. 先验证 ALSA 参数协商

```
arecord -D hw:0,1 \
    --dump-hw-params \
    -t raw -f S32_LE -r 192000 -c 2 -d 1 \
    /dev/null

echo "arecord rc=$?"
```

成功时：

```
arecord rc=0
```

同时检查有没有内核错误：

```
dmesg | tail -n 80
```

不能出现：

```
read error: Input/output error
xrun
DMA error
IRQ error
```

验证时使用 `hw:0,1`，不要使用 `plughw:`，否则 ALSA 软件转换可能掩盖硬件参数错误。

## 5. 使用已知信号测试 CPU 数据接收

推荐输入稳定的差分正弦波：

```
采样率：192 kHz
测试频率：30 kHz
波形：正弦波
幅度：必须在模拟前端和 AK5538 差分输入允许范围内
直流偏置：符合前级电路要求
```

30 kHz 在 192 kHz 下每周期约有：

```
192000 / 30000 = 6.4 个采样点
```

足以验证频率和串行接收。若前级在 30 kHz 附近衰减明显，也可以使用 24 kHz；24 kHz 对 8192 点 FFT 还是整数频点。

录制5秒：

```
rm -f /mnt/ak5538-cpu-test.wav

arecord -D hw:0,1 \
    -t wav -f S32_LE -r 192000 -c 2 -d 5 \
    /mnt/ak5538-cpu-test.wav

rc=$?
sync
echo "arecord rc=$rc"
wc -c /mnt/ak5538-cpu-test.wav
```

注意使用 `wc -c`，不要使用 `wc -l`。

5秒理论 PCM 数据量：

```
192000 × 2 × 4 × 5 = 7,680,000 bytes
```

加上 WAV 头，常见文件大小约为：

```
7,680,044 bytes
```

不同 `arecord` 版本的 WAV 头可能略有差异，因此允许几十字节差异。如果明显小于该值，说明录音提前结束、发生错误或参数不一致。

## 6. 检查文件基本信息

如果板上有 `soxi`：

```
soxi /mnt/ak5538-cpu-test.wav
```

预期：

```
Channels       : 2
Sample Rate    : 192000
Precision      : 32-bit
Duration       : 00:00:05
```

也可以使用：

```
file /mnt/ak5538-cpu-test.wav
```

检查录音过程日志：

```
dmesg | grep -iE 'bach2|wdma|dma|xrun|i/o error|irq|capture'
```

## 7. 判断采样内容是否真正正确

将 WAV 文件传到电脑，用 Audition 打开并进行频谱分析：

```
采样率：192000 Hz
通道：立体声
位深：32-bit
FFT：8192 或更大
窗口：Hann
```

输入30 kHz时应看到：

- 主峰位于约30 kHz；
- 左右声道的频率均正确，或符合实际接线；
- 波形连续，没有随机跳变；
- 没有削顶成平顶；
- 没有异常大的直流偏置；
- 噪声底明显低于主峰；
- 没有把30 kHz错误显示为其他频率。

常见异常对应关系：

| 现象                   | 可能原因                                |
| ---------------------- | --------------------------------------- |
| 文件大小正确但全零     | SDI选错、数据线未连接或通道未上电       |
| 文件大小正确但全是噪声 | I²S/MSB格式不一致、采样边沿错误、位偏移 |
| 主峰频率比例错误       | LRCK/采样率声明和实际时钟不一致         |
| 左右通道交错或相同     | LRCK极性、通道映射或模拟输入接线错误    |
| 周期性尖峰/断裂        | BCLK毛刺、DMA丢数、IRQ/xrun             |
| 波形上下削平           | 模拟输入幅度过大                        |
| `arecord -EIO`         | CPU RX、DMA、IRQ或BCLK/LRCK未正常运行   |

## 8. 验证通道映射

CPU当前只接收一个双通道数据线，因此应确认具体对应AK5538哪两个ADC通道。

建议分两次测试：

1. 只给预期左通道输入30 kHz，右通道保持安静；
2. 只给预期右通道输入30 kHz，左通道保持安静。

检查结果应当是：

```
测试1：左通道30 kHz主峰明显，右通道只有串扰/噪声
测试2：右通道30 kHz主峰明显，左通道只有串扰/噪声
```

如果给AK5538其他未连接到当前SDTO数据线的输入通道加信号，CPU录不到是正常的。

最终判定标准是：寄存器为 `0x07/0x00/0x00`、时钟为24.576 MHz/12.288 MHz/192 kHz、5秒文件大小正确、内核无DMA错误，并且FFT主峰与输入频率及通道完全对应。满足这些条件，才能认为CPU I²S接收正确。