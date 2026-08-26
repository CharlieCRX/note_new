# ACM8816 record 调用 AK5538 配置体系分析与调试指南

## 1. 结论

结合当前 DTS、驱动代码和《ACM8816 驱动修改与 AK5538 arecord 调试历程总结》，这套配置体系可以走通“通过 ACM8816 的 record/arecord 入口采集 AK5538 数据”的方向，但它不是内核真正通过 AK5538 codec 驱动去调用 AK5538。

当前实际方案是：

```text
arecord
  -> ALSA Capture PCM
  -> ASoC dai_link: bach1 + acm8816-hifi
  -> acm8816 codec DAI 提供 capture 能力，并在 capture 方向跳过 ACM8816 I2C
  -> sstar_bach 配置 SSD2351 I2S RX
  -> sstar_pcm 配置并启动 AI DMA
  -> SSD2351 I2S RX 接收 AK5538 SDTO 输出的数据
```

因此，“通过 acm8816 record 调用 ak5538”应理解为：

- ACM8816 驱动只是借用为 ASoC codec 端，用来让 ALSA 声卡和 Capture PCM 能创建出来。
- AK5538 没有作为 ASoC codec 节点出现在设备树里，也没有由内核 codec 驱动自动配置。
- AK5538 必须由外部脚本先通过 I2C/FPGA 配好采样率、MCLK、复位状态。
- arecord 真正采到的是 SSD2351 I2S RX + AI DMA 从 AK5538 SDTO 接收到的数据。

当前还未完全闭环，主要剩余两个核心问题：

1. `acm8816_dai.capture.rates` 不支持 128000 Hz，导致 `arecord -r 128000` 被 ALSA 协商成 192000 Hz。
2. AI DMA attach/open/start/interrupt 的初始化状态仍不稳定，出现 `bIsAttached FALSE` 和中断 Kernel Oops。

## 2. DTS 配置分析

### 2.1 当前声卡绑定关系

`pcupid.dtsi` 中启用的是 `asoc_sound`：

```dts
asoc_sound {
    compatible = "sstar, asoc-card";
    sstar-audio-card,name = "sigmastar,asoc-card";
    status = "okay";
    sstar-audio-card,cpu {
        sound-dai = <&bach1>;
    };
    sstar-audio-card,codec {
        sound-dai = <&acm8816>;
    };
};
```

这说明当前 ALSA 声卡的 CPU DAI 是 `bach1`，codec DAI 是 `acm8816`。

### 2.2 ACM8816 节点

`pcupid.dtsi` 中 `acm8816` 节点位于 I2C-1：

```dts
i2c1: i2c@1f222a00 {
    status = "okay";

    acm8816: acm8816@40 {
        compatible = "acm8816";
        reg = <0x40>;
        clocks = <&CLK_mck_12p288m>;
        clock-names = "mclk";
        #sound-dai-cells = <0>;
    };
};
```

这里没有 AK5538 的 ASoC codec 节点。也就是说，内核声卡拓扑中并不知道 AK5538 是 codec 端。

### 2.3 bach1 节点

```dts
bach1: bach1 {
    compatible = "sstar,bach";
    sound-dma = <&bach_dma1>;
    mclk0-freq = <12288000>;
    #sound-dai-cells = <0>;
    reg = <0x0 0x1F2A0400 0x0 0x800>;
    status = "okay";
};
```

`bach1` 是 SSD2351 内部音频/I2S 控制器。Capture 场景下，它负责配置 I2S RX 和相关时钟参数。

## 3. Machine Driver 分析

`sstar_asoc_card.c` 会解析 DTS 中的 `sstar-audio-card,cpu` 和 `sstar-audio-card,codec`。

关键逻辑：

```c
np_cpu_d0 = of_parse_phandle(np_tmp, "sound-dai", 0);
np_codec = of_parse_phandle(np_tmp, "sound-dai", i);
```

当 codec 节点没有 `codec-name`，但 `compatible = "acm8816"` 时，driver 会硬编码 DAI 名称：

```c
if (strcmp(compatible, "acm8816") == 0) {
    strncpy(codec_name, "acm8816-hifi", CODEC_NAME_BYTES - 1);
}
```

然后填充 dai_link：

```c
card->dai_link[i].codecs->of_node    = np_codec;
card->dai_link[i].codecs->dai_name   = codec_name;
card->dai_link[i].cpus->of_node      = np_cpu_d0;
card->dai_link[i].platforms->of_node = np_cpu_d0;
```

所以最终注册出来的 ASoC 链路就是：

```text
CPU DAI      = bach1
Codec DAI    = acm8816-hifi
Platform DMA = bach1 对应的 sstar_pcm
```

## 4. ACM8816 Codec Driver 分析

### 4.1 Capture 能力来自 acm8816_dai

`acm8816.c` 中定义了 `acm8816_dai`：

```c
static struct snd_soc_dai_driver acm8816_dai = {
    .name = "acm8816-hifi",
    .playback = {
        .channels_min = 1,
        .channels_max = 1,
        .rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
        .formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
    },
    .capture = {
        .channels_min = 1,
        .channels_max = 1,
        .rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
        .formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
    },
    .ops = &acm8816_dai_ops,
};
```

这段 `.capture` 是 arecord 能看到 capture PCM 设备的关键。没有它，ASoC 不会给这个 dai_link 创建录音子流。

### 4.2 Capture 方向已经跳过 ACM8816 I2C

当前 `acm8816_pcm_hw_params()` 中已经加入：

```c
if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
    dev_info(&priv->i2c->dev,
             "Capture: skip I2C config, AK5538 already configured by script\n");
    return 0;
}
```

这意味着 Capture 时不会继续写 ACM8816 寄存器。这个修改解决的是：

```text
Enter acm8816_pcm_hw_params
err: i2c-1 no ack
```

也就是说，Capture 链路不再把 AK5538 采集过程误当成 ACM8816 配置过程。

## 5. CPU DAI 与 PCM/DMA 分析

### 5.1 sstar_bach 配置 I2S RX

`sstar_bach_hw_params()` 中：

```c
if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
    ss_pcm_stream = &ss_pcm->rx;
    _sstar_ai_interface_config_set(platform, &tI2sCfg);
}
```

`_sstar_get_config_from_params()` 会从 ALSA 参数得到采样率：

```c
i2s_cfg.commonCfg.u32Rate = params_rate(params);
i2s_cfg.commonCfg.u32DmaRate = params_rate(params);
```

因此，如果 ALSA 协商成 192000 Hz，CPU I2S RX 也会按 192000 Hz 配置。

### 5.2 sstar_pcm 配置 AI DMA

Capture 方向在 `sstar_pcm_hw_params()` 中会走：

```c
mhal_audio_ai_config(ss_pcm_stream->dma_id, &ss_pcm_stream->pcm);
mhal_audio_ai_open(ss_pcm_stream->dma_id);
mhal_audio_ai_attach(&tAiAttach);
```

trigger START 时会调用：

```c
mhal_audio_ai_start(ss_pcm_stream->dma_id);
```

中断处理路径中会调用：

```c
mhal_audio_ai_interrupt_get(ss_pcm_stream->dma_id, &irq_status, &irq_flag);
mhal_audio_ai_get_curr_datalen(ss_pcm_stream->dma_id, &len);
```

这就是文档中 `bIsAttached FALSE` 和中断 Oops 的直接相关路径。

## 6. AK5538 外部初始化要求

由于 DTS/ASoC 中没有 AK5538 codec 节点，AK5538 必须先由脚本配置。

根据《AK5538 独立测试 — 方案 A》：

- AK5538 I2C 总线：I2C-1
- AK5538 I2C 地址：`0x10`
- FPGA 地址：`0x55`
- AD 时钟选择寄存器：`0x2`
- 复位控制寄存器：`0x5`
- AD 复位位：bit2

128K 配置重点：

```bash
fpga -w 0x2 0
fpga -w 0x5 0x1B
sleep 0.6
fpga -w 0x5 0x1F

i2cset -y 1 0x10 0x01 0x00
i2cset -y 1 0x10 0x02 0x05
i2cset -y 1 0x10 0x01 0x01

fpga -w 0x2 6
```

128K 理论信号：

```text
MCLK = 16.384 MHz
BCLK = 8.192 MHz
LRCK = 128 kHz
```

如果 AK5538 没有先配好，arecord 即使启动，也只能打开 CPU I2S RX/DMA，无法让 AK5538 自动进入正确输出状态。

## 7. 当前报错与根因分析

### 7.1 Capture 调用 ACM8816 I2C 导致 no ack

现象：

```text
Enter acm8816_pcm_hw_params
err: i2c-1 no ack
```

根因：

早期 Capture 方向进入 `acm8816_pcm_hw_params()` 后仍执行 ACM8816 寄存器配置，导致 I2C 访问失败。

当前状态：

代码已在 Capture 方向 `return 0`，此问题理论上已经解决。

验证方式：

```bash
dmesg | grep "Capture: skip I2C config"
```

预期看到：

```text
Capture: skip I2C config, AK5538 already configured by script
```

如果仍看到 `i2c-1 no ack`，说明当前板上运行的 kernel/module 不是这版代码，或代码没有被正确编译/加载。

### 7.2 128000 被协商成 192000

现象：

```text
arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 5 test.wav
requested 128000 -> got 192000
```

根因：

`acm8816_dai.capture.rates` 只声明：

```c
SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000
```

不包含 128000。ALSA 参数协商时无法接受 128000，只能选择邻近支持值，最终 CPU I2S RX 被配置为 192000 Hz。

影响：

```text
AK5538 LRCK = 128 kHz
CPU I2S RX  = 192 kHz
```

两端采样率不一致，会导致数据错位、变速、乱码，甚至影响后续 DMA 行为判断。

解决方向：

让 codec DAI 的 capture 侧支持 AK5538 实际采样率。历程总结中建议：

```c
.rates = SNDRV_PCM_RATE_48000 |
         SNDRV_PCM_RATE_96000 |
         SNDRV_PCM_RATE_192000 |
         SNDRV_PCM_RATE_KNOT,
.rate_min = 8000,
.rate_max = 192000,
```

如果只验证 128K，也可以更收敛地只把 Capture 侧能力扩展到需要的范围，但从 AK5538 支持多采样率的角度，使用 `SNDRV_PCM_RATE_KNOT` 更合适。

### 7.3 DMA buffer 越界 Oops

历程总结中的第一轮 Oops：

```text
pc : __memset
lr : snd_pcm_hw_params
bufferbyte[384000]
```

根因：

旧逻辑预分配 256KB DMA buffer，但 ALSA 根据用户参数请求约 384KB。ALSA Core 对 `dma_area` 做 `memset(dma_area, 0, dma_bytes)` 时越界。

当前状态：

当前 `sstar_pcm.c` 已改为根据 `params_buffer_bytes()` 动态分配，并注册 `hw_free` 释放，理论上该问题已解决。

### 7.4 AI DMA attach / 中断 Oops

现象：

```text
[ALSA ERROR][mhal_audio_ai_start]: enAiDma [0] bIsAttached FALSE
Unable to handle kernel paging request
sstar_snd_irq_handler
  -> mhal_audio_ai_get_curr_datalen
  -> HalAudDmaGetLevelCnt
  -> CamOsSpinLockIrqSave
```

根因判断：

`mhal_audio_ai_start()` 时 HAL 认为对应 AI DMA 尚未完成 attach，或者 attach 虽然调用了但 HAL 内部上下文没有真正建立完成。随后中断处理进入 `mhal_audio_ai_interrupt_get()` / `mhal_audio_ai_get_curr_datalen()`，访问了未初始化的 DMA 上下文指针。

当前代码中 Capture 初始化顺序为：

```text
mhal_audio_ai_config()
mhal_audio_ai_open()
mhal_audio_ai_attach()
trigger START
mhal_audio_ai_start()
enable interrupt
```

历程总结中建议的方向是确保：

```text
config -> attach -> open -> start
```

并且要确认 attach 成功、DMA 上下文完全初始化后，再允许 start 和中断处理读取 HAL DMA 状态。

## 8. 建议调试步骤

### 8.1 确认当前 kernel 已包含 Capture 跳过 I2C

执行：

```bash
dmesg -c
arecord -D hw:0,0 -r 192000 -c 1 -f S32_LE -d 1 /tmp/test.wav
dmesg | grep -E "acm8816|Capture|i2c-1 no ack"
```

预期：

```text
acm8816: Enter acm8816_pcm_hw_params
Capture: skip I2C config, AK5538 already configured by script
```

不应再出现：

```text
i2c-1 no ack
```

### 8.2 先用 192K 验证 DMA 路径，绕开 128K 协商问题

由于当前 DAI 支持 192K，可以先让 AK5538 也配置成 192K：

```bash
/tmp/test_ak5538.sh 192
arecord -D hw:0,0 -r 192000 -c 1 -f S32_LE -d 5 /tmp/ak5538_192k.wav
```

这样可以先排除“采样率不一致”对 DMA/Oops 的干扰。

如果 192K 下仍出现 `bIsAttached FALSE` 或中断 Oops，说明问题重点在 AI DMA attach/open/start/interrupt，而不是采样率。

### 8.3 再解决 128K 协商问题

修改方向：

```c
.capture = {
    .channels_min = 1,
    .channels_max = 1,
    .rates = SNDRV_PCM_RATE_48000 |
             SNDRV_PCM_RATE_96000 |
             SNDRV_PCM_RATE_192000 |
             SNDRV_PCM_RATE_KNOT,
    .rate_min = 8000,
    .rate_max = 192000,
    .formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
},
```

验证：

```bash
/tmp/test_ak5538.sh 128
arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 5 /tmp/ak5538_128k.wav
```

预期 dmesg 中：

```text
acm8816_pcm_hw_params - rate: 128000 Hz
sstar-bach: rate[128000]
```

不应再出现：

```text
requested 128000 -> got 192000
```

### 8.4 排查 AI DMA attach

建议在 `sstar_pcm_hw_params()` 中围绕以下调用增加日志：

```c
mhal_audio_ai_config()
mhal_audio_ai_attach()
mhal_audio_ai_open()
```

需要确认：

- 每个函数返回值都是 0。
- `tAiAttach.enAiDma` 与后续 `mhal_audio_ai_start(ss_pcm_stream->dma_id)` 使用的 DMA ID 一致。
- `tAiAttach.aenAiIf[0] = E_MHAL_AI_IF_I2S_RX_A_0_1` 与硬件实际 AK5538 接入的 I2S RX 通道一致。
- attach 发生在 `mhal_audio_ai_start()` 之前。
- 中断使能发生在 start 成功之后。

如果 HAL 要求 attach 在 open 之前，应调整为：

```text
mhal_audio_ai_config()
mhal_audio_ai_attach()
mhal_audio_ai_open()
mhal_audio_ai_start()
```

### 8.5 排查中断 Oops

重点观察：

```c
sstar_snd_irq_handler()
  -> mhal_audio_ai_interrupt_get()
  -> sstar_pcm_ack()
  -> mhal_audio_ai_get_curr_datalen()
  -> sstar_process_irq()
```

建议验证：

- open 阶段 `request_irq()` 后是否可能收到共享中断。
- IRQ handler 中是否需要判断 `runtime_state == SSTAR_PCM_RUNTIME_STATE_RUNNING` 后才访问 AI DMA。
- AI DMA 未 attach 或未 start 时，是否应该直接跳过 `mhal_audio_ai_interrupt_get()` / `mhal_audio_ai_get_curr_datalen()`。
- STOP/close/hw_free 后是否还可能有残留中断访问已释放的 DMA buffer 或 HAL 上下文。

## 9. 推荐验证矩阵

| 阶段 | AK5538 配置 | arecord 参数 | 目标 |
| --- | --- | --- | --- |
| A | 192K | `-r 192000 -c 1 -f S32_LE` | 先绕开 128K 协商问题，验证 DMA/中断 |
| B | 128K | `-r 128000 -c 1 -f S32_LE` | 验证修改 rates 后是否能保持 128K |
| C | 48K | `-r 48000 -c 1 -f S32_LE` | 验证标准采样率路径 |
| D | 96K | `-r 96000 -c 1 -f S32_LE` | 验证当前已声明采样率路径 |

每轮都建议同步检查：

```bash
dmesg | grep -E "acm8816|sstar-bach|mhal_audio_ai|AI attach|Oops|no ack"
```

示波器检查：

```text
MCLK 是否等于 AK5538 脚本目标值
BCLK 是否等于目标值
LRCK 是否等于 arecord 协商后的实际采样率
SDTO 是否持续输出
```

## 10. 最终判断

当前体系的设计思路是合理的，但它是一个“借 ACM8816 codec DAI 建立 ALSA Capture 入口，实际采集 AK5538 I2S 输出”的集成方案。

要真正稳定实现 arecord 采集 AK5538，需要满足四个条件：

1. DTS 中 `asoc_sound` 绑定 `bach1 + acm8816`，并能正常注册声卡。
2. `acm8816_dai` 必须声明 capture 能力，且 capture 方向不能再访问 ACM8816 I2C。
3. AK5538 必须在 arecord 前由脚本正确配置，并且 MCLK/BCLK/LRCK/SDTO 物理信号正常。
4. ALSA 协商采样率、CPU I2S RX 采样率、AK5538 LRCK 三者必须一致，同时 AI DMA attach/open/start/interrupt 状态必须完整。

当前代码已经完成了第 1、2 项的大部分工作，AK5538 独立脚本文档覆盖了第 3 项。剩余重点是第 4 项：先修采样率能力，再修 AI DMA attach/中断时序。
