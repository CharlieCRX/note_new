| 文档版本     | V2.0                                           |
| :------- | :--------------------------------------------- |
| **日期**   | 2026-07-14                                     |
| **适用平台** | USBL（SSD2351 + FPGA + ACM8816 + AK5538）        |
| **前置条件** | 已完成 I2S0 6-Wire 硬件配置验证，AK5538 可通过 Shell 脚本正常工作 |


## 一、方案概述

### 1.1 当前状态

| 项目            | 状态     | 说明                              |
| :-------------- | :------- | :-------------------------------- |
| AK5538 硬件配置 | ✅ 可用   | Shell 脚本 + 示波器验证通过       |
| ACM8816 播放    | ✅ 可用   | 0x40 被内核占用，`aplay` 可工作   |
| AK5538 内核集成 | ❌ 不可用 | `arecord` 崩溃，报 `i2c-1 no ack` |
| 6-Wire 独立时钟 | ✅ 已验证 | TX 和 RX 时钟独立输出             |

### 1.2 方案目标

让 `arecord -D hw:0,0` 能够成功录制 AK5538 的音频数据，同时**不影响 ACM8816 的播放功能**，从而验证：
1. RX 数据路径（AK5538 → SSD2351 I2S RX → DMA → ALSA）是否完整
2. TX 与 RX 是否能同时独立工作（全双工 6-Wire 模式）

### 1.3 核心策略

**修改 `acm8816.c` 驱动**，使录音（Capture）方向跳过 I2C 操作，播放（Playback）方向保持不变。

| 方向 | Codec 驱动 | I2C 操作 | 硬件配置来源 |
| :--- | :--- | :--- | :--- |
| **播放（TX）** | `acm8816.c` | ✅ 正常执行 | 内核驱动自动配置 ACM8816 |
| **录音（RX）** | `acm8816.c` | ❌ 跳过（直接返回成功） | Shell 脚本配置 AK5538 |

**优点**：
- ✅ `acm8816Launch` 播放程序**完全不受影响**，ACM8816 仍由内核驱动自动配置
- ✅ `arecord` 不再报 I2C 错误，ASoC 链路顺利启动
- ✅ AK5538 由 Shell 脚本独立配置，与内核解耦
- ✅ 无需修改 DTS，无需依赖 Dummy Codec


## 二、前置条件确认

### 2.1 硬件确认

```bash
# 1. 确认 I2C 设备可见
i2cdetect -y 1
# 应看到: 0x10 (AK5538), 0x40 (ACM8816, UU), 0x55 (FPGA)

# 2. 确认 AK5538 可通过脚本正常工作
./test_ak5538.sh 128
# 示波器测量 MCLK/LRCK/BCLK 应正常
```

### 2.2 内核源码准备

确保你有 `acm8816.c` 的源码，并可以重新编译内核或内核模块。


## 三、`acm8816.c` 驱动修改方案

### 3.1 修改位置

文件：`sound/soc/codecs/acm8816.c`

函数：`acm8816_pcm_hw_params`

### 3.2 修改内容

在函数开头添加录音方向判断，如果是 Capture 方向则直接返回成功，**不执行任何 I2C 操作**。

**修改前：**

```c
static int acm8816_pcm_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params,
                                 struct snd_soc_dai *dai)
{
    struct snd_soc_component *component = dai->component;
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    unsigned int rate = params_rate(params);
    unsigned int channels = params_channels(params);
    unsigned int format = params_format(params);
    int sample_width = snd_pcm_format_width(format);
    unsigned char format_reg = 0;

    // ... 后续 I2C 配置代码 ...
}
```

**修改后：**

```c
static int acm8816_pcm_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params,
                                 struct snd_soc_dai *dai)
{
    struct snd_soc_component *component = dai->component;
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    unsigned int rate = params_rate(params);
    unsigned int channels = params_channels(params);
    unsigned int format = params_format(params);
    int sample_width = snd_pcm_format_width(format);
    unsigned char format_reg = 0;

    // ===== 新增：录音方向跳过 I2C =====
    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
        dev_info(&priv->i2c->dev,
                 "Capture direction: skip I2C config (AK5538 configured by external script)\n");
        return 0;  // 直接返回成功，不操作 I2C
    }
    // ==================================

    // 播放方向：正常执行 I2C 配置
    // ... 原有 I2C 配置代码 ...
}
```

### 3.3 完整修改示例

```c
static int acm8816_pcm_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params,
                                 struct snd_soc_dai *dai)
{
    struct snd_soc_component *component = dai->component;
    struct acm8816_priv *priv = snd_soc_component_get_drvdata(component);
    unsigned int rate = params_rate(params);
    unsigned int channels = params_channels(params);
    unsigned int format = params_format(params);
    int sample_width = snd_pcm_format_width(format);
    unsigned char format_reg = 0;
    int ret = 0;

    dev_info(&priv->i2c->dev,
             "acm8816_pcm_hw_params - rate: %u Hz, channels: %u, stream: %s\n",
             rate, channels,
             substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "Playback" : "Capture");

    /*
     * 录音方向：跳过 I2C 配置
     * AK5538 由外部 Shell 脚本（init_ak5538.sh）通过 I2C 和 FPGA 独立配置
     * 内核仅需启动 SSD2351 的 I2S RX 和 DMA
     */
    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
        dev_info(&priv->i2c->dev,
                 "Capture: skip I2C config, AK5538 already configured by script\n");
        return 0;
    }

    /* ========== 以下为播放方向的 I2C 配置（原有逻辑，保持不变） ========== */

    switch (sample_width) {
    case 16:
        format_reg |= 0x00;
        break;
    case 32:
        format_reg |= 0x03;
        break;
    default:
        dev_err(&priv->i2c->dev, "Unsupported sample width: %d bits\n", sample_width);
        return -EINVAL;
    }

    /* 设置 I2S 数据格式 */
    regmap_write(priv->regmap, 0x00, 0x00);
    regmap_write(priv->regmap, ACM8816_REG_I2S_DATA_FORMAT1, format_reg);

    /* 播放方向：使能 Codec 上电 */
    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
        acm8816_set_bias_level(component, SND_SOC_BIAS_ON);
    }

    return 0;
}
```

### 3.4 工作原理

```text
                    ┌─────────────────────────────────────┐
                    │         acm8816_pcm_hw_params      │
                    └────────────────┬────────────────────┘
                                     │
                    ┌────────────────┴────────────────┐
                    │                                 │
                    ▼                                 ▼
           ┌────────────────┐              ┌────────────────┐
           │  Playback 方向  │              │   Capture 方向  │
           │ (substream->   │              │ (substream->   │
           │  stream ==     │              │  stream ==     │
           │  PLAYBACK)     │              │  CAPTURE)      │
           └───────┬────────┘              └───────┬────────┘
                   │                                 │
                   ▼                                 ▼
           ┌────────────────┐              ┌────────────────┐
           │ ✅ 正常执行     │              │ ✅ 直接返回 0   │
           │ I2C 配置       │              │ 跳过 I2C 操作  │
           │ ACM8816 寄存器 │              │                │
           └───────┬────────┘              └───────┬────────┘
                   │                                 │
                   ▼                                 ▼
           ┌────────────────┐              ┌────────────────┐
           │ ACM8816 被     │              │ AK5538 由外部  │
           │ 内核驱动配置   │              │ Shell 脚本配置 │
           │ ✅ 播放正常    │              │ ✅ 录音正常    │
           └────────────────┘              └────────────────┘
```

### 3.5 编译与部署
参考：
- [[TFTP烧录内核]]
- [[编译kernel]]

## 四、AK5538 初始化脚本

### 4.1 脚本内容

保存为 `/usr/bin/init_ak5538.sh`：

```bash
#!/bin/sh
#==============================================================================
# AK5538 初始化脚本（USBL 平台）
# 在每次录音前执行，配置 AK5538 的采样率和时钟
#==============================================================================

I2C_BUS=1
AK5538_ADDR=0x10

# FPGA 寄存器（USBL）
REG_AD_CLK_SEL=0x2
REG_RST_CTL=0x5
RST_AD_ASSERT=0x1B
RST_AD_DEASSERT=0x1F

# 默认采样率 128K
FS=${1:-128}

# 采样率 -> FS 编码
get_fs_code() {
    case $1 in
        32)   echo "0x6d" ;;
        33)   echo "0x35" ;;
        34)   echo "0x1d" ;;
        35)   echo "0x1d" ;;
        36)  echo "0x05" ;;
        37)  echo "0x05" ;;
        38)  echo "0x55" ;;
        39)  echo "0x45" ;;
        40)  echo "0x65" ;;
        41)  echo "0x65" ;;
        *)    echo "0x05" ;;
    esac
}

# 采样率 -> MCLK 枚举值
get_mclk() {
    case $1 in
        32)   echo "8" ;;
        33)   echo "7" ;;
        34)   echo "6" ;;
        35)   echo "7" ;;
        36)  echo "6" ;;
        37)  echo "7" ;;
        38)  echo "7" ;;
        39)  echo "7" ;;
        40)  echo "8" ;;
        41)  echo "9" ;;
        *)    echo "6" ;;
    esac
}

FS_CODE=$(get_fs_code $FS)
MCLK=$(get_mclk $FS)

echo "=== AK5538 初始化 (${FS}K, FS_CODE=${FS_CODE}, MCLK=${MCLK}) ==="

# 1. 关闭 AD 时钟
fpga -w $REG_AD_CLK_SEL 0

# 2. 复位 AD（保持其他设备解复位）
fpga -w $REG_RST_CTL $RST_AD_ASSERT
sleep 0.6

# 3. 解复位 AD
fpga -w $REG_RST_CTL $RST_AD_DEASSERT

# 4. 配置 AK5538 寄存器
i2cset -y $I2C_BUS $AK5538_ADDR 0x01 0x00  # Power_Ma2 关闭
i2cset -y $I2C_BUS $AK5538_ADDR 0x02 $FS_CODE  # Control_1
i2cset -y $I2C_BUS $AK5538_ADDR 0x01 0x01  # Power_Ma2 开启

# 5. 设置 MCLK
fpga -w $REG_AD_CLK_SEL $MCLK

# 6. 回读验证
echo -n "Control_1 = "
i2cget -y $I2C_BUS $AK5538_ADDR 0x02
echo -n "AD 时钟选择 = "
fpga -r $REG_AD_CLK_SEL

echo "=== AK5538 初始化完成 ==="
```

### 4.2 添加执行权限

```bash
chmod +x /usr/bin/init_ak5538.sh
```


## 五、验证步骤

### 5.1 验证驱动修改生效

重启后，执行 `arecord` 并观察内核日志：

```bash
# 清空内核日志
dmesg -c

# 执行录音
arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 1 /dev/null

# 查看内核日志
dmesg | tail -20
```

预期应看到类似日志：

```text
acm8816_pcm_hw_params - rate: 128000 Hz, channels: 1, stream: Capture
Capture: skip I2C config, AK5538 already configured by script
```

**不应再出现 `i2c-1 no ack` 或 Kernel Oops。**

### 5.2 验证 ACM8816 播放功能不受影响

```bash
# 1. 播放测试音频
aplay -D hw:0,0 -r 48000 -c 2 -f S16_LE test_play.wav

# 2. 检查内核日志
dmesg | grep "acm8816_pcm_hw_params"
```

预期应看到：

```text
acm8816_pcm_hw_params - rate: 48000 Hz, channels: 2, stream: Playback
```

**播放应正常出声，无 I2C 错误。**

### 5.3 执行录音测试

```bash
# 1. 初始化 AK5538（128K 采样率）
init_ak5538.sh 128

# 2. 执行录音
arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 5 test.wav
```

### 5.4 预期结果

| 现象                               | 含义                                |
| :--------------------------------- | :---------------------------------- |
| `arecord` 正常完成，无 Kernel Oops | ✅ I2C 错误已消除，ASoC 链路成功启动 |
| 生成 `test.wav` 文件               | ✅ ALSA 层已收到数据                 |
| 文件大小 > 0                       | ✅ DMA 正在搬运数据                  |
| 播放录音有声音（非全零/白噪声）    | ✅ AK5538 → I2S → DMA 完整链路正常   |
| `acm8816Launch` 播放正常           | ✅ ACM8816 播放功能不受影响          |


## 六、6-Wire 独立收发验证

### 6.1 单项测试

| 测试        | 命令                                                         | 验证方法                              |
| :---------- | :----------------------------------------------------------- | :------------------------------------ |
| **TX 播放** | `aplay -D hw:0,0 test_play.wav`                              | 示波器测 TX_SDO（PAD_GPIOD_03）有数据 |
| **RX 录音** | `init_ak5538.sh 128 && arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 5 test.wav` | 示波器测 RX_SDI（PAD_OUTP_CH3）有数据 |

### 6.2 全双工测试（6-Wire 核心验证）

```bash
# 终端 1：持续播放（ACM8816）
aplay -D hw:0,0 -r 128000 -c 1 -f S32_LE /path/to/test_audio.wav

# 终端 2：同时录音（AK5538）
init_ak5538.sh 128 && arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 10 full_duplex_test.wav
```

**通过标准**：
- 播放声音正常，无卡顿/断音（ACM8816 正常工作）
- 录音文件有效，无播放信号串扰（AK5538 独立工作）
- 示波器上 TX_SDO 和 RX_SDI 同时有数据波形

### 6.3 物理测量点速查

| 信号        | 引脚          | 测量点            |
| :---------- | :------------ | :---------------- |
| **TX_BCLK** | PAD_PWM_ADC00 | SSD2351 → ACM8816 |
| **TX_LRCK** | PAD_PWM_ADC01 | SSD2351 → ACM8816 |
| **TX_SDO**  | PAD_GPIOD_03  | SSD2351 → ACM8816 |
| **RX_BCLK** | PAD_OUTP_CH2  | SSD2351 → AK5538  |
| **RX_LRCK** | PAD_OUTN_CH2  | SSD2351 → AK5538  |
| **RX_SDI**  | PAD_OUTP_CH3  | AK5538 → SSD2351  |


## 七、修改前后对比

### 7.1 代码修改对比

| 对比项           | 修改前                               | 修改后                                       |
| :--------------- | :----------------------------------- | :------------------------------------------- |
| **播放（Playback）** | 执行 I2C 配置 ACM8816，成功后上电   | ✅ **不变**，正常执行 I2C 配置 ACM8816       |
| **录音（Capture）**  | 执行 I2C 配置 ACM8816 → 芯片未上电 → `no ack` → 崩溃 | ✅ 检测到 Capture 方向，直接返回成功，跳过 I2C |
| **AK5538**          | 未被内核使用                         | ✅ 由 Shell 脚本独立配置，数据流入内核        |

### 7.2 功能对比

| 场景                     | 修改前 | 修改后 |
| :----------------------- | :----- | :----- |
| `acm8816Launch` 播放     | ✅ 正常 | ✅ 正常（不受影响） |
| `aplay` 播放             | ✅ 正常 | ✅ 正常（不受影响） |
| `arecord` 录音           | ❌ 崩溃 | ✅ 可运行（需先执行 `init_ak5538.sh`） |
| 同时播放 + 录音（6-Wire） | ❌ 不可行 | ✅ 可验证 |

### 7.3 信号流总览

```text
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                         应用层                                 │
                    │  ┌─────────────────┐    ┌─────────────────────────────────┐   │
                    │  │ acm8816Launch   │    │          arecord                │   │
                    │  │   (播放程序)    │    │        (录音程序)               │   │
                    │  └────────┬────────┘    └────────────────┬────────────────┘   │
                    └───────────┼─────────────────────────────┼─────────────────────┘
                                │                             │
                    ┌───────────┼─────────────────────────────┼─────────────────────┐
                    │           ▼                             ▼                      │
                    │  ┌─────────────────────────────────────────────────────┐   │
                    │  │                    ALSA Core                        │   │
                    │  └─────────────────────────────────────────────────────┘   │
                    │           │                             │                   │
                    │           ▼                             ▼                   │
                    │  ┌─────────────────────────────────────────────────────┐   │
                    │  │              ASoC Core (soc-pcm.c)                 │   │
                    │  └─────────────────────────────────────────────────────┘   │
                    │           │                             │                   │
                    └───────────┼─────────────────────────────┼───────────────────┘
                                │                             │
                    ┌───────────┼─────────────────────────────┼───────────────────┐
                    │           ▼                             ▼                   │
                    │  ┌─────────────────────────────────────────────────────┐   │
                    │  │              Machine Driver                       │   │
                    │  │          (sstar_asoc_card.c)                      │   │
                    │  │      DAI Link: bach1 ←→ acm8816-hifi             │   │
                    │  └─────────────────────────────────────────────────────┘   │
                    │           │                             │                   │
                    │           ▼                             ▼                   │
                    │  ┌─────────────────────────────────────────────────────┐   │
                    │  │              Codec Driver (acm8816.c)              │   │
                    │  │                                                   │   │
                    │  │  播放方向: 执行 I2C 配置 ACM8816                   │   │
                    │  │  录音方向: 跳过 I2C，直接返回成功                  │   │
                    │  └─────────────────────────────────────────────────────┘   │
                    └───────────┬─────────────────────────────────────────────────┘
                                │
                    ┌───────────┼─────────────────────────────────────────────────┐
                    │           │                                                  │
                    │           ▼                                                  │
                    │  ┌─────────────────────────────────────────────────────┐   │
                    │  │              CPU DAI (sstar_bach.c)                │   │
                    │  │      配置 I2S 时钟，启动 TX 和 RX DMA              │   │
                    │  └─────────────────────────────────────────────────────┘   │
                    └───────────┬─────────────────────────────────────────────────┘
                                │
                    ┌───────────┼─────────────────────────────────────────────────┐
                    │           │                                                  │
                    │           ▼                                                  │
                    │  ┌─────────────────────────────────────────────────────┐   │
                    │  │              SSD2351 I2S0 硬件                     │   │
                    │  │                                                   │   │
                    │  │  TX_SDO ──────────────────► ACM8816 (0x40)        │   │
                    │  │  RX_SDI ◄────────────────── AK5538 (0x10)         │   │
                    │  └─────────────────────────────────────────────────────┘   │
                    │           │                             │                   │
                    │           ▼                             ▼                   │
                    │  ┌─────────────────┐    ┌─────────────────────────────────┐│
                    │  │   ACM8816       │    │      AK5538 (Shell 脚本配置)    ││
                    │  │ 由内核 I2C 配置  │    │   init_ak5538.sh 配置寄存器    ││
                    │  │ ✅ 播放正常      │    │   ✅ 录音数据正常               ││
                    │  └─────────────────┘    └─────────────────────────────────┘│
                    └─────────────────────────────────────────────────────────────┘
```


## 八、常见问题排查

### 8.1 录音仍报 I2C 错误

**现象**：修改后 `arecord` 仍报 `i2c-1 no ack`

**排查**：
1. 确认修改的代码已正确编译并部署
2. 检查内核日志确认 `Capture: skip I2C config` 日志是否出现
3. 检查 `acm8816_pcm_hw_params` 是否被正确调用

### 8.2 录音文件全零

**现象**：`arecord` 成功，但播放 `test.wav` 无声或全零

**排查**：
```bash
# 1. 确认 AK5538 已正确初始化
i2cget -y 1 0x10 0x02  # 应输出配置的 FS 编码

# 2. 确认 FPGA SDTO 方向（0x23=1 表示输出到 SSD2351）
fpga -r 0x23

# 3. 确认 I2S 时钟到达 AK5538
# 示波器测量: MCLK (Pin 24), BCLK, LRCK

# 4. 确认 AK5538 PDN 引脚为高电平
```

### 8.3 播放功能受影响

**现象**：修改后 `acm8816Launch` 或 `aplay` 播放异常

**排查**：
1. 确认代码修改中 `if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)` 判断正确
2. 确认播放方向的代码路径未被修改
3. 检查内核日志确认播放方向执行了 I2C 配置

### 8.4 Kernel Oops 仍出现

**现象**：修改后仍发生 Segmentation fault

**排查**：
- 检查 `sstar_pcm.c` 中 DMA Buffer 分配（`alloc_dmem()` → `msys_request_dmem()`）
- 确认 `runtime->dma_area` 是否为有效的内核虚拟地址
- 检查 `snd_pcm_hw_params` 中的 `__memset` 操作是否访问了非法地址


## 九、成功标志

完成本方案后，应满足以下所有条件：

| #    | 检查项                                         | 状态 |
| :--- | :--------------------------------------------- | :--- |
| 1    | `arecord` 不再报 `i2c-1 no ack`                | ☐    |
| 2    | `arecord` 不再发生 Kernel Oops                 | ☐    |
| 3    | 内核日志显示 `Capture: skip I2C config`        | ☐    |
| 4    | `acm8816Launch` 播放功能正常                   | ☐    |
| 5    | `aplay` 播放功能正常                           | ☐    |
| 6    | 示波器测量 RX_SDI 有数据波形                   | ☐    |
| 7    | `arecord` 生成的 wav 文件可播放且有内容        | ☐    |
| 8    | 同时 `aplay` + `arecord` 互不干扰              | ☐    |


## 十、总结

| 问题                | 解决方案                                                     |
| :------------------ | :----------------------------------------------------------- |
| 录音时 `acm8816.c` 执行 I2C 失败 | 在 `acm8816_pcm_hw_params` 中判断方向，录音时跳过 I2C 操作   |
| ACM8816 播放受影响  | 播放方向的 I2C 配置逻辑**完全保留**，不受影响                |
| AK5538 未被内核使用 | 由 Shell 脚本独立初始化 AK5538，内核仅启动 I2S RX 和 DMA     |
| 6-Wire 独立收发验证 | 同时运行 `aplay` 和 `arecord`，示波器测量 TX/RX 数据引脚     |

**核心原则**：
- **播放（TX）**：ACM8816 由内核 `acm8816.c` 驱动通过 I2C 自动配置，保证 `acm8816Launch` 正常工作
- **录音（RX）**：AK5538 由 Shell 脚本通过 I2C + FPGA 独立配置，内核跳过 I2C 操作仅启动数据搬运
- 两者在驱动层面相互独立，在硬件层面共享 SSD2351 I2S 控制器但使用独立的 TX/RX 通道

> 本文档基于 USBL 平台 I2S0 6-Wire 调试过程整理，执行完成后请更新各检查项状态。