# AK5538 驱动保留与用户态 I2C+FPGA 共存方案

## 1. 需求和结论

当前需求包含三个目标：

1. 保留 `sound/soc/codecs/ak5538.c` 和相关 Kconfig/Makefile，以便保存已经完成的 AK5538 codec 驱动成果。
2. 当前产品运行时不让该驱动绑定物理 AK5538，不让内核占用 I2C-1 地址 `0x10`。
3. 保留一个可用于 `arecord` 的 ASoC capture PCM；物理 AK5538 的寄存器、复位和 MCLK 全部由用户态的 `i2c-tools` 与 `fpga` 工具配置。

推荐架构是：

```text
                    ASoC 逻辑链路
用户态 arecord  <-> BACH2 CPU DAI <-> sstar-dummy-codec2
                         |
                         +-> I2S_RX_A -> WDMA1

                    物理硬件链路
用户态 fpga 工具  -> FPGA 0x55 -> MCLK/外部复位
用户态 i2c-tools  -> AK5538 0x10 -> AK5538 寄存器
AK5538            -> BCLK/LRCK/SDTO -> BACH2 I2S RX
```

关键原则：

> 保留 AK5538 驱动源码或内核模块，不等于必须让它绑定 `1-0010`。驱动可以存在，但 DTS 不为它创建 I2C client；录音的 ASoC 逻辑端点由 dummy codec 提供。

## 2. 当前 `UU` 的原因

当前 `i2cdetect -y 1` 显示：

```text
10: UU -- -- -- -- -- -- -- -- -- -- -- 1c -- -- --
40: UU -- -- -- -- -- -- 47 48 -- -- -- -- -- -- --
```

其中：

- `0x10` 的 `UU` 是 AK5538 DT I2C client 已经创建并被 `ak5538` 驱动绑定；
- `0x40` 的 `UU` 是 ACM8816 被内核驱动占用，这是正常现象，不需要释放。

当前 DTS 中的根因是：

```dts
ak5538: ak5538@10 {
    compatible = "ak5538";
    reg = <0x10>;
    #sound-dai-cells = <0>;
    status = "okay";
};
```

以及主声卡引用：

```dts
sstar-audio-card,codec {
    sound-dai = <&acm8816>, <&ak5538>;
};
```

因此系统启动时：

```text
DT 创建 1-0010 I2C client
  -> compatible 匹配 ak5538_i2c_driver
  -> codec probe
  -> 内核保留 0x10
  -> i2c-tools 显示 UU
```

## 3. 为什么不能只执行 `unbind`

运行时可能尝试：

```sh
echo 1-0010 > /sys/bus/i2c/drivers/ak5538/unbind
```

该命令只解除驱动与 client 的绑定，不一定删除 DT 创建的 I2C client。只要 `1-0010` client 仍注册在 I2C core 中，`i2c-dev` 就可能继续认为地址已被占用。

本内核 `drivers/i2c/i2c-core-base.c` 的 `delete_device` 接口只允许删除通过 `new_device` sysfs 接口创建的用户态 client，不能用来安全删除 DT 创建的 client。

因此不应把下面的流程当作正式方案：

```text
unbind -> delete_device -> i2cset
```

也不推荐使用：

```sh
i2cset -f ...
```

`-f` 会绕过内核地址占用检查。如果 codec 驱动、regmap、runtime PM 或 ASoC 回调仍可能访问 AK5538，就会产生用户态和内核同时写寄存器的竞争。

正式方案必须从 DTS 层阻止 `1-0010` client 创建，并在烧录新 DTB 后重新启动。

## 4. DTS 修改方案

### 4.1 禁用物理 AK5538 的内核 client

为了尽量保留公共 `pcupid.dtsi` 中的 AK5538 节点定义，可以在板级 DTS：

```text
arch/arm64/boot/dts/sstar/pcupid-ssm001c-s01a-voip.dts
```

增加或修改为：

```dts
&ak5538 {
    status = "disabled";
};
```

并删除当前板级固定参数：

```dts
ak5538,fixed-sample-rate = <192000>;
ak5538,fixed-channels = <2>;
```

使用 `status = "disabled"` 后：

- AK5538 驱动仍然可以编译进内核或保留为模块；
- DT 不再创建活动的 `1-0010` client；
- AK5538 codec probe 不会执行；
- 用户态可以通过 `/dev/i2c-1` 访问物理地址 `0x10`；
- `i2cdetect` 中 `0x10` 应显示 `10`，而不是 `UU`。

如果该板永远不会使用内核 AK5538 codec，也可以直接删除 `ak5538@10` DT 节点；但从“保留驱动成果”的角度看，公共节点保留、板级禁用更方便以后重新启用。

### 4.2 ASoC link 改用 dummy codec

不能简单从声卡中删除第二个 codec phandle，否则 BACH2 capture runtime 没有 codec DAI，通常不会生成 PCM。

当前主声卡：

```dts
asoc_sound {
    sstar-audio-card,cpu {
        sound-dai = <&bach1>, <&bach2>;
    };

    sstar-audio-card,codec {
        sound-dai = <&acm8816>, <&ak5538>;
    };
};
```

建议改为：

```dts
asoc_sound {
    sstar-audio-card,cpu {
        sound-dai = <&bach1>, <&bach2>;
    };

    sstar-audio-card,codec {
        sound-dai = <&acm8816>, <&dummy_codec2>;
    };
};
```

这样可以尽量保持当前 ALSA 结构：

```text
card 0, device 0：ACM8816 playback
card 0, device 1：BACH2 I2S RX capture
```

最终设备通常仍为：

```text
/dev/snd/pcmC0D0p
/dev/snd/pcmC0D1c
```

实际编号仍应以 `arecord -l` 和 `/proc/asound/pcm` 为准。

### 4.3 给 dummy link 增加通用属性

建议扩展现有 dummy codec 节点：

```dts
dummy_codec2: dummy_codec2 {
    compatible = "sstar,dummy-codec2";
    codec-name = "sstar-dummy-codec2";
    sstar,capture-only;
    sstar,codec-clock-provider;
    #sound-dai-cells = <0>;
    status = "okay";
};
```

含义为：

- `sstar,capture-only`：该 link 只创建 capture substream；
- `sstar,codec-clock-provider`：物理线路的远端设备提供 BCLK/LRCK，CPU 是 consumer/slave。

这两个属性是建议新增的私有绑定，需要在 `sstar_asoc_card.c` 中实现解析。

也可以把属性放到 BACH2 或 card link 节点，但机器驱动当前已经逐个解析 codec phandle，因此放在 `dummy_codec2` 上改动最小。

### 4.4 保留 I2S0 RX 和 DMA 配置

以下板级配置需要保留：

```dts
&sound {
    i2s-rx0-tdm-mode = <2>;       /* CPU slave */
    i2s-rx0-tdm-fmt = <1>;        /* I2S */
    i2s-rx0-tdm-wiremode = <2>;   /* 6-wire */
    i2s-rx0-channel = <2>;
};
```

并保留：

```dts
bach_dma2: bach_dma2 {
    rdma = <1>;
    wdma = <1>;
    status = "okay";
};
```

BACH2/WDMA1 属于 CPU 采集通路，不属于 AK5538 codec 驱动，不能随 AK5538 的解绑一起删除。

### 4.5 明确外部 MCLK

当前板级 DTS 中：

```dts
&bach2 {
    mclk0-freq = <24576000>;
};
```

但物理 MCLK 实际由 FPGA 输出，因此建议改为：

```dts
&bach2 {
    sstar,external-mclk;
    sstar,capture-source = "i2s-rx-a";
};
```

对应驱动语义：

- `sstar,external-mclk`：BACH2 不操作 SoC MCLK 输出；
- `sstar,capture-source`：BACH2 capture 使用 I2S_RX_A。

这些属性也需要在 `sstar_bach.c` 中实现。

## 5. Kconfig 和驱动保留策略

### 5.1 保留 AK5538 驱动

保留以下文件和构建项：

```text
sound/soc/codecs/ak5538.c
sound/soc/codecs/ak5538.h
sound/soc/codecs/Kconfig 中的 SND_SOC_AK5538
sound/soc/codecs/Makefile 中的 snd-soc-ak5538
```

推荐配置为模块：

```text
CONFIG_SND_SOC_AK5538=m
```

这样：

- 驱动成果仍然保留并能单独生成 `snd-soc-ak5538.ko`；
- 当前板 DTS 禁用节点时，即使模块被加载也不会绑定 `0x10`；
- 以后需要恢复内核管理时，只需启用正确 DT 节点并加载模块。

保留为内建也可以：

```text
CONFIG_SND_SOC_AK5538=y
```

只要 DT 节点是 disabled，内建驱动同样不会绑定 `1-0010`。模块形式只是更便于验证和隔离。

### 5.2 启用 SigmaStar dummy codec

当前配置中：

```text
# CONFIG_SND_SOC_SSTAR_DUMMY_CODEC is not set
```

需要改为：

```text
CONFIG_SND_SOC_SSTAR_DUMMY_CODEC=y
```

该驱动是 platform dummy codec，不挂在 I2C 总线上，因此不会占用 `0x10`。

## 6. `sstar_asoc_card.c` 修改方案

当前机器驱动只有识别到 AK5538 compatible 时才执行：

- `capture_only=1`；
- `playback_only=0`；
- `I2S | NB_NF | CBP_CFP`；
- 使用 `ak5538-hifi`。

替换为 dummy codec 后，这些 AK5538 专用分支不会执行。因此应把“capture-only”和“远端提供时钟”从 codec 型号判断改为 DTS 属性判断。

建议逻辑：

```c
if (of_property_read_bool(np_codec, "sstar,capture-only")) {
    card->dai_link[i].capture_only = 1;
    card->dai_link[i].playback_only = 0;
}

if (of_property_read_bool(np_codec, "sstar,codec-clock-provider")) {
    card->dai_link[i].dai_fmt =
        SND_SOC_DAIFMT_I2S |
        SND_SOC_DAIFMT_NB_NF |
        SND_SOC_DAIFMT_CBP_CFP;
}
```

同时保留 `codec-name` 解析，使 dummy codec DAI 名称为：

```text
sstar-dummy-codec2
```

原先用于补建第二个 PCM 的 `late_probe()` 建议暂时保留。因为当前仍采用主声卡的第二条 normal link，删除它可能重新出现：

```text
runtime[1]: pcm=no
```

只有在 dummy link 已经确认能够由 vendor ASoC core 自动创建 PCM 后，才考虑移除该兼容逻辑。

## 7. `sstar_bach.c` 修改方案

### 7.1 删除“依赖 AK5538 DAI 名称”的采集路由

当前只有发现：

```text
codec_dai.name = ak5538-hifi
```

才会启用：

```text
I2S_RX_A
AI_MCH_01 -> E_I2S_RXA_01
```

改用 dummy codec 后，该分支不会运行，PCM 虽可能存在，但不会建立物理 I2S RX 数据路径。

应改为根据 BACH2 DTS：

```dts
sstar,capture-source = "i2s-rx-a";
```

或者根据 capture substream 和 BACH2 实例通用地启用：

```c
if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
    platform->capture_source == SSTAR_CAPTURE_I2S_RX_A) {
    platform->ai_path_select[E_AUD_AI_MCH_01].status = true;
    platform->ai_path_select[E_AUD_AI_MCH_01].action = E_I2S_RXA_01;
}
```

### 7.2 dummy codec 不调用 codec sysclk

现有代码已经通过名称前缀跳过 dummy codec 的 `snd_soc_dai_set_sysclk()`，该行为应保留：

```text
sstar-dummy-codec*
```

dummy codec 没有物理 MCLK，不应该调用 codec sysclk。

### 7.3 外部 MCLK 时跳过 SoC MCLK 设置

当前 `sstar_bach_setup()` 会对所有 MCLK ID 调用：

```c
mhal_audio_mclk_setting(i, mclk0, TRUE);
```

建议在 `sstar,external-mclk` 模式下跳过该循环：

```c
if (!platform->external_mclk) {
    /* existing mhal_audio_mclk_setting() loop */
}
```

用户态通过 FPGA `0x2` 选择物理 AK5538 MCLK。

### 7.4 清理强制 TDM8

当前 AK5538 专用 `hw_params()` 分支仍强制：

```text
CPU slave
6-wire
TDM8
8 channels
```

改用 dummy codec 后该分支不会执行，但不能只依赖“分支不执行”来获得正确配置。建议建立通用的外部 I2S RX 配置：

```c
if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
    platform->capture_source == SSTAR_CAPTURE_I2S_RX_A) {
    tI2sCfg.enMode = E_MHAL_AUDIO_MODE_TDM_SLAVE;
    tI2sCfg.enFmt = E_MHAL_AUDIO_I2S_FMT_I2S;
    tI2sCfg.en4WireMode = E_MHAL_AUDIO_4WIRE_OFF;
    tI2sCfg.u16Channels = params_channels(params);
    tI2sCfg.u16Width = params_width(params);
}
```

对于当前测试，结果应为：

```text
rate     = 192000
channels = 2
width    = 32
CPU      = slave
wire     = 6-wire
format   = Normal I2S
```

## 8. `sstar_pcm.c` 修改保留范围

以下修改必须保留：

- BACH1 映射 RDMA0/WDMA0；
- BACH2 映射 RDMA1/WDMA1；
- `hw_params()` 动态分配 DMA buffer；
- 将 DMA 地址同步给 ALSA runtime；
- 双通道 capture attach `I2S_RX_A_0_1` 到 WDMA1；
- stop/close 时 detach 和释放 DMA；
- `request_irq()` 使用有效设备名称。

这些都是 CPU PCM/DMA 的通用修复，与 AK5538 是否由内核管理无关。

## 9. 用户态 AK5538 配置流程

禁用内核 I2C client 后，由一个统一脚本串行完成 FPGA、AK5538 和 `arecord` 配置。

当前已经验证通过的 192 kHz、双通道、S32_LE 配置可按以下顺序执行：

```sh
# 1. FPGA：AK5538 外部复位
fpga -w 0x5 0x1B

# 2. FPGA：MCLK = 24.576 MHz
fpga -w 0x2 7
sleep 0.1

# 3. FPGA：AK5538 外部解复位
fpga -w 0x5 0x1F

# 4. AK5538：内部时序复位
i2cset -y 1 0x10 0x01 0x00

# 5. AK5538：192 kHz、2 x 32-bit Normal I2S
i2cset -y 1 0x10 0x00 0xFF
i2cset -y 1 0x10 0x02 0x07
i2cset -y 1 0x10 0x03 0x00
i2cset -y 1 0x10 0x04 0x00

# 6. AK5538：释放内部时序复位
i2cset -y 1 0x10 0x01 0x01
sleep 0.02

# 7. 启动 CPU PCM 接收
arecord -D hw:0,1 \
    -t wav -f S32_LE -r 192000 -c 2 -d 5 \
    /mnt/ak5538-192k-stereo.wav
```

实际 card/device 号必须以 `arecord -l` 为准。

用户态工具必须保证：

- FPGA MCLK、AK5538 CKS 和 `arecord -r` 一致；
- AK5538 数据格式和 BACH2 I2S 格式一致；
- 录音过程中禁止修改 MCLK 或 AK5538 时序寄存器；
- 使用锁文件或 `flock` 防止多个测试程序并发；
- suspend/resume 或 FPGA/ADC 复位后重新初始化；
- 配置后回读 FPGA 和 AK5538 寄存器。

## 10. ES7243 占位策略

不建议让 AK5538 驱动伪装成 ES7243 驱动，也不建议把 AK5538 的 compatible 或 DAI 名称用于 ES7243 占位。

两颗芯片的：

- I2C 寄存器；
- 时钟比例；
- 电源时序；
- DAI 格式；
- 通道能力；
- reset/standby 逻辑；

都可能不同。复用错误驱动会让后续调试无法区分是 ASoC 链路问题还是 codec 寄存器问题。

推荐的生命周期为：

```text
现在：
    BACH2 + sstar-dummy-codec2
    AK5538 由用户态独立配置

未来接入 ES7243：
    BACH2 + es7243 codec driver
    DTS compatible 改为 ES7243 的真实 compatible
    dummy_codec2 从 link 中移除

保留的 AK5538 驱动：
    只作为独立、可重新启用的 AK5538 驱动成果
    不作为 ES7243 的代码占位
```

如果这里所说的“ES7243 占位”只是希望预留一个 capture PCM，那么 dummy codec 正是正确的占位方式。

## 11. 推荐修改文件清单

| 文件 | 修改内容 |
| --- | --- |
| `arch/arm64/boot/dts/sstar/pcupid.dtsi` | 主声卡第二 codec 从 `ak5538` 改为 `dummy_codec2`；为 dummy/BACH2 增加通用测试属性 |
| `arch/arm64/boot/dts/sstar/pcupid-ssm001c-s01a-voip.dts` | `&ak5538` 改为 disabled；删除 fixed-rate/fixed-channels；声明外部 MCLK 和 I2S_RX_A |
| `sound/soc/sstar/pcupid/sstar_asoc_card.c` | 根据 DTS 属性设置 capture-only、CBP_CFP，不再依赖 AK5538 compatible |
| `sound/soc/sstar/pcupid/sstar_bach.c` | 通用化 I2S_RX_A、CPU slave/6-wire/双通道配置；外部 MCLK 时跳过 SoC MCLK；去除采集路径对 `ak5538-hifi` 名称的依赖 |
| `sound/soc/sstar/pcupid/sstar_pcm.c` | 保留当前 DMA/AI attach 修复，原则上无需因本方案回退 |
| `sound/soc/codecs/Kconfig` | 保留 `SND_SOC_AK5538`；启用 `SND_SOC_SSTAR_DUMMY_CODEC` |
| `sound/soc/codecs/Makefile` | 保留 AK5538 和 dummy codec 构建项 |
| defconfig/config fragment | `AK5538=m` 或 `y`，`SSTAR_DUMMY_CODEC=y` |

## 12. 分阶段实施顺序

不要一次性删除绑定、替换 codec、改路由和切换用户态脚本。推荐按以下顺序实施：

1. 启用 `CONFIG_SND_SOC_SSTAR_DUMMY_CODEC=y`。
2. 修改机器驱动，使 dummy codec link 能正确标记 capture-only 和 CBP_CFP。
3. 修改 BACH2，使 I2S_RX_A、外部 MCLK和双通道配置不依赖 `ak5538-hifi`。
4. 主声卡第二 link 从 AK5538 替换为 dummy_codec2，但暂时保留 AK5538 DT client。
5. 验证 dummy capture PCM 能创建、BACH2 使用 WDMA1。
6. 板级 DTS 将 `&ak5538` 设置为 disabled，重新编译并烧录 DTB。
7. 重启确认 `0x10` 不再显示 `UU`。
8. 用用户态脚本配置 FPGA 和 AK5538。
9. 执行 192 kHz 双通道录音、FFT 和长时间稳定性测试。
10. 最后将 AK5538 驱动配置调整为期望的 `m` 或 `y` 保留形式。

## 13. 验证方法

### 13.1 验证 I2C 地址已经释放

```sh
ls -l /sys/bus/i2c/devices/1-0010
readlink /sys/bus/i2c/devices/1-0010/driver
i2cdetect -y 1
```

正式目标是：

- 不存在活动的 `/sys/bus/i2c/devices/1-0010` DT client；
- `0x10` 显示为 `10`，而不是 `UU`；
- `0x40` 继续显示 `UU`，因为 ACM8816 仍由内核管理。

不建议频繁扫描整条生产 I2C 总线。确认地址后，日常验证优先使用定点读：

```sh
i2cget -y 1 0x10 0x01
i2cget -y 1 0x10 0x02
```

### 13.2 验证 AK5538 驱动仍然保留

模块方式：

```sh
modinfo snd-soc-ak5538
```

配置检查：

```sh
zcat /proc/config.gz | grep SND_SOC_AK5538
```

保留驱动但禁用 DT 节点后，启动日志中不应再出现：

```text
AK5538 codec driver probed at I2C addr 0x10
```

### 13.3 验证 dummy capture PCM

```sh
cat /proc/asound/pcm
arecord -l
ls -l /dev/snd
dmesg | grep -iE 'runtime\[|dummy|bach2|PCM DMA mapping|AI attach'
```

应看到：

- BACH2 对应 capture PCM；
- `pcmC0D1c` 或等价的 capture 节点；
- BACH2 使用 `wdma=1`；
- `AI attach succeeded for dma_id 1, channels 2`。

### 13.4 验证物理时钟和录音

192 kHz 双通道 S32_LE 时：

```text
MCLK = 24.576 MHz
BCLK = 12.288 MHz
LRCK = 192 kHz
SDTO = 有连续数据
```

录音：

```sh
arecord -D hw:0,1 \
    -t wav -f S32_LE -r 192000 -c 2 -d 5 \
    /mnt/ak5538-192k-stereo.wav
```

5 秒 PCM 数据理论值：

```text
192000 × 2 × 4 × 5 = 7,680,000 bytes
```

## 14. 最终结论

最终推荐状态为：

```text
AK5538 codec driver：保留源码和构建项，但不绑定 1-0010
AK5538 DT node：当前板 status = disabled
ASoC capture codec：sstar-dummy-codec2
CPU capture：BACH2 + I2S_RX_A + WDMA1
物理 AK5538 配置：用户态 i2c-tools
MCLK/外部复位：用户态 fpga 工具
未来 ES7243：使用真正的 ES7243 codec 驱动替换 dummy codec
```

这种方案同时满足：

- AK5538 驱动成果得到保留；
- 用户态可以安全访问 I2C `0x10`；
- 内核仍然提供稳定的 `arecord` capture PCM；
- 测试 ADC 与未来正式 ES7243 驱动互不混淆；
- 后续恢复内核 AK5538 管理时，只需重新启用对应 DT 节点和声卡 link。
