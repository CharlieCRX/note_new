# ACM8816 驱动修改与 AK5538 arecord 调试历程总结

| 文档版本 | V2.0 |
| :--- | :--- |
| **更新日期** | 2026-07-16 |
| **适用平台** | USBL（SSD2351 + FPGA + ACM8816 + AK5538） |
| **任务目标** | 修改 ACM8816 驱动使之能够使用 `arecord` 来调用 AK5538 |

---

## 一、项目背景与目标

### 1.1 系统拓扑

```
           换能器
    ↑                ↓
 发射声波          接收回波

 ACM8816          AK5538
    ↑                ↓
   SSD2351 (主控/DSP)
```

- **ACM8816**：将 SSD2351 生成的数字发射波形通过 I2S 接收，转换并放大成模拟信号，驱动换能器发射声波。
- **AK5538**：8 通道 Audio ADC（I2S Slave），将换能器接收的回波模拟信号转为 I2S 数字音频流，发送给 SSD2351。

### 1.2 已有基础

在本次任务开始时，SDK 已具备以下能力：

> **SSD2351 I2S TX → ACM8816 的播放链路已打通。**

具体包含：
- SSD2351 I2S0 能够通过 I2S TX 向 ACM8816 发送 PCM 数据
- 提供了 `acm8816Launch` 用户态程序，可发送指定频率的正弦波
- 提供了替换用的 kernel 文件集（含 `acm8816.c`、`sstar_asoc_card.c`、`sstar_bach.c`、`sstar_pcm.c` 及 DTS 文件）

### 1.3 任务目标

> 验证 **SSD2351 I2S0 在 6-Wire 模式下 RX 和 TX 收发功能**。

具体分为：
1. 通过 FPGA 模拟的 RX 数据进行 I2S0 RX 调试（AK5538 作为 ADC 输入源）
2. 使用 `arecord` 录音验证整条 Capture 链路

---

## 二、系统架构概览

### 2.1 SDIK 四层组成

```
boot        → U-Boot
kernel      → Linux Kernel
project     → 打包 Image、Rootfs、ko
sdk         → 用户态 Demo / API / 应用
```

### 2.2 替换的 Kernel 文件

```
kernel
├── arch/arm64/boot/dts/sstar/
│   ├── pcupid-ssm001c-s01a-voip-padmux.dtsi     # PadMux 配置
│   ├── pcupid-ssm001c-s01a-voip.dts             # 板级 DTS
│   └── pcupid.dtsi                              # SoC 级 DTS
├── arch/arm64/configs/
│   └── pcupid_ssm001c_s01a_spinand_voip_defconfig
└── sound/soc/
    ├── codecs/
    │   ├── acm8816.c          # ACM8816 Codec 驱动
    │   └── acm8816.h
    └── sstar/pcupid/
        ├── sstar_asoc_card.c   # Machine 驱动
        ├── sstar_bach.c        # CPU DAI 驱动 (I2S Controller)
        └── sstar_pcm.c         # PCM/DMA 驱动
```

### 2.3 ALSA ASoC 架构中各组件的职责

| 文件 | 角色 | 核心职责 |
|------|------|----------|
| `acm8816.c` | Codec 驱动 | **定义"能力"**：声明 I2S DAI 支持何种格式/速率/通道数；通过 I2C 配置 ACM8816 寄存器；**不传输 PCM 数据** |
| `sstar_bach.c` | CPU DAI 驱动 | **控制 I2S 硬件**：配置 BCLK/LRCK/位宽/FIFO；操作 SSD2351 内部 I2S Controller 寄存器 |
| `sstar_pcm.c` | PCM/DMA 驱动 | **搬运数据**：分配 DMA Buffer；配置并启动 DMA；在 DDR 与 I2S FIFO 之间搬运 PCM 数据 |
| `sstar_asoc_card.c` | Machine 驱动 | **描述拓扑**：通过 DTS 解析谁（CPU）连接了谁（Codec），填充 `dai_link`，注册声卡 |

### 2.4 两条总线的关系

```
       I2C 总线                   I2S 总线
    (配置芯片)                  (传输音频)

 SSD2351 ────→ ACM8816      SSD2351 ────→ ACM8816
   │ 写寄存器 0x10 = 0x18       │ BCLK / LRCK / SDOUT
   │ 配置音量/采样率/静音        │ PCM 数据流
   └── 只发指令,不传音频         └── 只传音频,不发指令
```

> I2C 负责"配置芯片"，I2S 负责"传输数据"。

### 2.5 TX/RX 方向约定

**TX 和 RX 永远相对于 SSD2351（主控）而言：**

```
SSD2351 TX  =  ACM8816 RX  →  播放
SSD2351 RX  =  AK5538  TX  →  录音
```

### 2.6 当前硬件拓扑

```text
                        SSD2351 (SoC)
                    ┌───────────────────────┐
                    │  I2S0 RX (SDIN)       │
                    │        ▲              │
                    │        │              │
                    │  I2C1  │              │
                    │  ┌─────┴──────┐       │
                    └──┴────────────┴───────┘
                       │            │
                       ▼            ▼
               ┌──────────────┐ ┌──────────────┐
               │  ACM8816     │ │  AK5538      │
               │  (0x40)      │ │  (0x10)      │
               │  Playback    │ │  Capture     │
               └──────────────┘ └──────────────┘
```

---

## 三、第一阶段：I2S0 6-Wire 配置验证（物理层确认）

### 3.1 验证链

```
PCB 管脚定义 → ARMTmux 映射 → PadMux DTS → Device Tree → Driver
```

### 3.2 PCB 布线确认

依据《USBL 水面设备管脚定义及器件地址说明》，确认 PCB 上定义了完整的 6 根 I2S0 信号：

| I2S 信号 | PAD |
|----------|-----|
| TX_BCLK | PAD_GPIOD_04 |
| TX_WCK | PAD_GPIOD_05 |
| TX_SDO0 | PAD_GPIOD_03 |
| RX_BCLK | PAD_OUTP_CH2 |
| RX_WCK | PAD_OUTN_CH2 |
| RX_SDI | PAD_OUTP_CH3 |

> 结论：PCB 已将 TX 与 RX 分别布线，符合 6-Wire 硬件连接方式。

### 3.3 ARMTmux 名称映射确认

USBL 文档使用 **PAD_Name**（硬件名称），Linux DTS 使用 **SW_Name**（软件名称），二者实际指向同一物理 PAD。

| PAD_Name (文档) | SW_Name (DTS) | I2S 功能 |
|-----------------|---------------|----------|
| PAD_GPIOD_04 | PAD_PWM_ADC00 | TX_BCK |
| PAD_GPIOD_05 | PAD_PWM_ADC01 | TX_WCK |
| PAD_GPIOD_03 | PAD_GPIOD_03 | TX_SDO0 |
| PAD_OUTP_CH2 | PAD_OUTP_CH2 | RX_BCK |
| PAD_OUTN_CH2 | PAD_OUTN_CH2 | RX_WCK |
| PAD_OUTP_CH3 | PAD_OUTP_CH3 | RX_SDI |

### 3.4 PadMux 配置确认

`pcupid-ssm001c-s01a-voip-padmux.dtsi` 中：
```dts
// I2S0
<PAD_OUTP_CH2   PINMUX_FOR_I2S0_RX_MODE_3   MDRV_PUSE_I2S0_RX_BCK>,
<PAD_PWM_ADC00  PINMUX_FOR_I2S0_4W_TX_MODE_1 MDRV_PUSE_I2S0_TX_BCK>,
...
// I2C1 (用于 I2C 配置)
<PAD_GPIOA_16   PINMUX_FOR_I2C1_MODE_3      MDRV_PUSE_I2C1_SCL>,
<PAD_GPIOA_17   PINMUX_FOR_I2C1_MODE_3      MDRV_PUSE_I2C1_SDA>,
```

> 结论：PadMux 配置与 PCB 一致，不存在配置错误。

### 3.5 Device Tree 6-Wire 模式声明

`pcupid.dtsi` 中已声明：
```dts
i2s-trx-shared-padmux = <0>;
i2s-tx-padmux = <6>;
i2s-rx-padmux = <7>;
i2s-tx0-tdm-wiremode = <2>;   // wiremode=2 → 6-Wire
i2s-rx0-tdm-wiremode = <2>;   // wiremode=2 → 6-Wire
```

### 3.6 第一阶段结论

| 验证项 | 结果 | 说明 |
|--------|------|------|
| PCB 布线 | ✔ 已确认 | TX、RX 已分别布线 |
| USBL 管脚定义 | ✔ 已确认 | 完整定义了 6 根 I2S 信号 |
| ARMTmux 映射 | ✔ 已确认 | PAD_Name ↔ SW_Name 一一对应 |
| PadMux DTS | ✔ 已确认 | 与 PCB 硬件连接一致 |
| Device Tree | ✔ 已确认 | 已声明 wiremode=2 (6-Wire) |

> **PCB → ARMTmux → PadMux DTS 的配置链已闭环。**

---

## 四、第二阶段：ALSA 驱动理论知识建设

在进行 RX 调试之前，系统学习了 Linux ALSA/ASoC 架构。

### 4.1 核心实验一：注释 Capture 对设备节点的影响

**实验操作**：在 `acm8816.c` 中注释掉 `snd_soc_dai_driver acm8816_dai` 的 `.capture` 成员。

**实验结果**：
- `aplay -l` → 仍显示播放设备
- `arecord -l` → **无任何设备**
- `/dev/snd/pcmC0D0c` → **消失**

**根因分析**：
```
sstar_asoc_card_probe()                  // Machine 驱动入口
  → devm_snd_soc_register_card()
    → snd_soc_bind_card()                // 声卡绑定
      → soc_init_pcm_runtime()
        → soc_new_pcm()                  // ★ PCM 创建工厂
          → soc_get_playback_capture()   // 读取 Codec DAI 中的 .playback/.capture
          → snd_pcm_new(..., playback, capture, ...)
            // 如果 capture=0，则不创建 Capture 子流
```

**核心结论**：
> PCM Device 不是由 Codec 驱动创建的，也不是由用户程序创建的，而是由 **ASoC Framework 根据 CPU DAI 和 Codec DAI 共同生成**。

### 4.2 Machine 驱动的 probe 机制

`sstar_asoc_card_probe` 的核心工作：

| 步骤 | 操作 | 关键数据 |
|------|------|----------|
| 解析 DTS | 找到 CPU 端 I2S 控制器 | `bach1` |
| 解析 DTS | 找到 Codec 端芯片 | `acm8816` |
| 名称匹配 | 驱动根据 `compatible="acm8816"` 硬编码为 `"acm8816-hifi"` | 必须与 `acm8816.c` 中 `.name` 一字不差 |
| 填充 `dai_link` | 将 CPU 和 Codec 节点填入链接表 | `card->dai_link[i].cpus` / `.codecs` |
| 调用注册 | `devm_snd_soc_register_card()` | 触发 ASoC 核心进入 PCM 创建流程 |

### 4.3 PCM Device 创建时机

完整调用链：
```
sstar_asoc_card_probe
  → devm_snd_soc_register_card
    → snd_soc_register_card
      → snd_soc_bind_card
        → snd_soc_add_pcm_runtime    // 匹配 Codec DAI 名称
        → soc_init_pcm_runtime
          → soc_new_pcm               // ★ PCM 设备在此诞生
            → soc_get_playback_capture
            → snd_pcm_new             // ALSA 核心 API，分配设备号
              → devtmpfs 生成 /dev/snd/pcmCXDXx
```

### 4.4 Codec 驱动的真实职责

- **定义能力**：`snd_soc_dai_driver` 声明支持的速率/格式/通道
- **注册组件**：`devm_snd_soc_register_component()` 向 ASoC 核心注册
- **提供操作**：`snd_soc_dai_ops` 中的 `hw_params`、`set_fmt`、`startup`、`shutdown`

> **Codec 驱动不调用 `snd_pcm_new`，不创建 `/dev/snd/` 节点，不搬运 PCM 数据。**

---

## 五、第三阶段：AK5538 独立测试（脱离 ALSA 验证硬件）

### 5.1 方法：fpga + i2c-tools Shell 脚本

无需编译任何代码，利用已安装在开发板上的 `fpga` 工具和 `i2c-tools` 即可完成 AK5538 配置。

| 操作目标 | I2C 总线 | 从地址 | 使用工具 |
|----------|----------|--------|----------|
| FPGA 寄存器 | I2C-1 | 0x55 | `fpga -r` / `fpga -w` |
| AK5538 寄存器 | I2C-1 | 0x10 | `i2cget` / `i2cset` |

### 5.2 BDZ → USBL 平台差异适配

| 项目 | BDZ | USBL |
|------|-----|------|
| I2C 总线 | `/dev/i2c-0` | `/dev/i2c-1` |
| AD 时钟选择寄存器 | `0x10` | `0x2` |
| 复位控制寄存器 | `0x01` | `0x5` |
| AD 复位位 | `0x01` bit1 | `0x5` bit2 |
| 复位方式 | 写 1，自动跳回 | 先写 `0x1B`(bit2=0)，再写 `0x1F`(bit2=1)，需手动恢复 |

### 5.3 AK5538 硬件工作流程

```
① PDN 解复位    (fpga -w 0x5 0x1F) →  LDO/偏置电路启动
② 寄存器配置    (i2cset)          →  采样率/格式设定
③ MCLK 注入     (fpga -w 0x2 6)   →  PLL 锁定，内部调制时钟生成
④ 硬件自初始化  (578 个 LRCK 周期) →  输出静音等待稳定
⑤ 模拟采样      (Δ-Σ 调制+抽取滤波)→  模拟→数字转换
⑥ I2S 输出      (BCLK/LRCK 驱动)  →  SDTO 引脚串行输出 PCM
```

### 5.4 测试脚本

编写了 `init_ak5538.sh` 一键脚本（即 `test_ak5538.sh`），支持所有采样率（32K~768K）的遍历测试。

---

## 六、第四阶段：集成方案设计 — 绕过 ACM8816 I2C

### 6.1 核心策略

**修改 `acm8816.c` 驱动**，使录音（Capture）方向跳过 I2C 操作，播放（Playback）方向保持不变。

| 方向 | Codec 驱动 | I2C 操作 | 硬件配置来源 |
| :--- | :--- | :--- | :--- |
| **播放（TX）** | `acm8816.c` | ✅ 正常执行 | 内核驱动自动配置 ACM8816 |
| **录音（RX）** | `acm8816.c` | ❌ 跳过（直接返回成功） | Shell 脚本配置 AK5538 |

### 6.2 修改内容

在 `sound/soc/codecs/acm8816.c` 的 `acm8816_pcm_hw_params` 函数开头添加：

```c
// 录音方向：跳过 I2C 配置
if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
    dev_info(&priv->i2c->dev,
             "Capture: skip I2C config, AK5538 already configured by script\n");
    return 0;  // 直接返回成功，不操作 I2C
}
// 播放方向：正常执行 I2C 配置（原有逻辑保持不变）
```

**效果**：
- ✅ 消除了 Capture 路径上的 `i2c-1 no ack` 错误
- ✅ ACM8816 播放功能完全不受影响

### 6.3 原理说明

```
acm8816_pcm_hw_params()
        │
   ┌────┴────┐
   │         │
 Playback  Capture
   │         │
   ▼         ▼
 I2C 配置   跳过 I2C
 ACM8816   return 0
   │         │
   ▼         ▼
 CPU I2S TX  CPU I2S RX + DMA → 采集 AK5538 的 SDTO 数据
```

**关键理解**：Codec 驱动的 `hw_params` 回调只是整个 ASoC `hw_params` 流程中的一个环节。跳过 I2C 操作不会跳过 CPU DAI（`sstar_bach.c`）的 I2S 时钟配置和 Platform（`sstar_pcm.c`）的 DMA 配置。AK5538 的 SDTO 数据仍然能通过已配置好的 I2S RX + DMA 流入内存。

---

## 七、第五阶段：arecord RX 调试 — 两轮迭代

### 7.1 数据流预期

```
FPGA (输出 MCLK)
       │
       ▼
AK5538 (ADC, I2S Slave)
       │
       │ SDTO (FPGA 0x23=1 时输出到 SSD2351)
       ▼
SSD2351 I2S RX → AI DMA → ALSA PCM Capture → arecord
```

### 7.2 第一轮调试：I2C 绕过前

执行：
```bash
arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 5 test.wav
```

**现象**：
1. 通道数错误：PCM 驱动仅支持 Mono，需改为 `-c 1`
2. 采样率警告：requested 128000 → got 192000
3. Segmentation fault + Kernel Oops

**Kernel Oops 分析**：
```
pc : __memset
lr : snd_pcm_hw_params
```
崩溃在 ALSA Core 对 DMA Buffer 执行 `memset` 清零时。

同时日志显示：
```
Enter acm8816_pcm_hw_params
err: i2c-1 no ack      ← Capture 链路调用了 ACM8816 I2C
```

> 结论：有两个独立问题 — **(1)** I2C NO ACK，(2) DMA Buffer 异常。

### 7.3 第二轮调试：绕过 I2C + DMA 动态分配

在第四阶段的 `acm8816.c` 修改基础上，进一步修改 `sstar_pcm.c`。

### 7.3.1 DMA 缓冲区越界根因分析

**`sstar_pcm_probe` 中的预分配**：
```c
int size_max = 512 * 512; // 固定 256KB
buf->area = alloc_dmem(name, PAGE_ALIGN(size_max), ...);
```

**`sstar_pcm_hw_params` 中的赋值**：
```c
substream->runtime->dma_bytes = params_buffer_bytes(hw_params);  // 384KB (用户请求)
substream->runtime->dma_area = ss_pcm_stream->buf.area;          // 只有 256KB 映射
```

**日志确认**：
```
bufferbyte[384000]   ← 请求 375KB > 预分配 256KB
```

> **根因**：ALSA Core 对 `dma_area` 执行 `memset(dma_area, 0, dma_bytes)` 时，`dma_bytes` (375KB) 远超实际映射的 `dma_area` (256KB)，导致越界访问未映射的虚拟地址 → Kernel Oops。

### 7.3.2 对 `sstar_pcm.c` 的修改

| 修改项 | 内容 | 效果 |
| :--- | :--- | :--- |
| 移除 probe 预分配 | 不再在 probe 中调用 `alloc_dmem` | 消除固定大小限制 |
| `hw_params` 动态分配 | 根据 `params_buffer_bytes()` 按需分配 | ✅ DMA 大小匹配请求，消除 `memset` 越界 |
| 添加 `mhal_audio_ai_attach` | 在 `hw_params` 中 config 之后、open 之前调用 attach | ⚠️ 部分解决 `bIsAttached FALSE` |
| 添加 `sstar_pcm_hw_free` | 释放 DMA 缓冲区 | ✅ 防止内存泄漏 |
| 注册 `hw_free` 回调 | 在 ops 表中添加 `.hw_free` | ✅ |

### 7.3.3 第二轮调试的实际日志

```text
enter ai power manage
sstar-bach: mclk = 12288000
sstar-bach: codec_dai.name = acm8816-hifi
acm8816: Enter acm8816_set_dai_sysclk - clk_id: 0, freq: 12288000
sstar-bach: Activating I2S_TX_A path for ACM8816
acm8816: Enter acm8816_pcm_startup - stream: Capture
acm8816: Enter acm8816_pcm_hw_params - rate: 192000 Hz, channels: 1
acm8816: Capture: skip I2C config, AK5538 already configured by script  ← ✅
sstar-bach: rate[192000] chanl[1], bufferbyte[384000], ...
Unable to handle kernel paging request at virtual address ffffffc009b5d000  ← ❌ 新 Oops
```

**新 Kernel Oops (DMA 动态分配后)**：
```
Call Trace:
  sstar_snd_irq_handler
    → mhal_audio_ai_get_curr_datalen
      → HalAudDmaGetLevelCnt
        → CamOsSpinLockIrqSave
```

与第一轮的 `__memset` Oops 不同，第二轮 Oops 发生在**中断处理函数**中，访问了未初始化的 HAL 层指针。

### 7.3.4 第二轮调试后仍存在的问题

**问题一：采样率协商不匹配**

| 项目 | 用户请求值 | 实际协商值 |
| :--- | :--- | :--- |
| 采样率 (`-r`) | 128000 Hz | **192000 Hz** |
| 硬件 LRCK（示波器） | — | **128000 Hz** |

**根因**：`acm8816_dai` 的 `rates` 字段定义为：
```c
.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000
```
**不包含 128000**。ALSA 核心将 128000 向上取整到最近的合法值 192000。

**影响**：CPU I2S 控制器被配置为 192000 Hz，但 AK5538 实际工作在 128000 Hz → 采样率不匹配。

**问题二：AI DMA 中断 Oops**

日志显示：
```
[ALSA ERROR][mhal_audio_ai_start]: enAiDma [0] bIsAttached FALSE
```

中断 `sstar_snd_irq_handler` 中访问了 `mhal_audio_ai_get_curr_datalen` → `HalAudDmaGetLevelCnt`，访问未初始化的 DMA 上下文指针。

---

## 八、代码修改汇总

### 8.1 全部代码修改清单

| 文件 | 修改内容 | 状态 |
| :--- | :--- | :--- |
| `sound/soc/codecs/acm8816.c` | `acm8816_pcm_hw_params` 添加 Capture 跳过 I2C | ✅ 已合并 |
| `sound/soc/sstar/pcupid/sstar_pcm.c` | `sstar_pcm_probe` 移除预分配 | ✅ 已合并 |
| `sound/soc/sstar/pcupid/sstar_pcm.c` | `sstar_pcm_hw_params` 添加动态分配 | ✅ 已合并 |
| `sound/soc/sstar/pcupid/sstar_pcm.c` | `sstar_pcm_hw_params` 添加 `mhal_audio_ai_attach` | ⚠️ 待验证 |
| `sound/soc/sstar/pcupid/sstar_pcm.c` | `sstar_pcm_hw_free` 添加释放逻辑 | ✅ 已合并 |
| `sound/soc/sstar/pcupid/sstar_pcm.c` | `sstar_pcmengine_register` 注册 `hw_free` | ✅ 已合并 |

### 8.2 当前状态汇总

| 检查项 | 状态 | 说明 |
| :--- | :--- | :--- |
| AK5538 硬件配置 | ✅ 正常 | 脚本配置成功，示波器 LRCK 实测 128kHz |
| ACM8816 I2C 错误 | ✅ 已消除 | 修改 `acm8816_pcm_hw_params`，Capture 跳过 I2C |
| ALSA Card 注册 | ✅ 成功 | PCM Device 已创建 |
| DMA 缓冲区越界 | ✅ 已消除 | 改为动态分配，大小匹配 |
| AI DMA attach | ⚠️ 部分完成 | 已添加调用，但中断仍访问非法地址 |
| 采样率协商 | ❌ 异常 | 128000 被 ALSA 协商为 192000 |
| Kernel Oops (中断) | ❌ 仍存在 | 中断处理函数访问未初始化指针 |
| 录音文件 | ❌ 无法验证 | 因 Kernel Oops 无法完成录音 |

---

## 九、问题依赖关系与根因分析

### 9.1 问题全景图

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                        问题依赖关系图                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  采样率协商问题 (128k → 192k)  ──────►  CPU 与 AK5538 采样率不匹配         │
│         │                                   │                               │
│         │                                   ▼                               │
│         │                          I2S 数据采样错位                         │
│         │                                   │                               │
│         │                                   ▼                               │
│         │                          录音数据无效 (变调/乱码)                 │
│         │                                                                   │
│   DMA/中断问题  ──────────────────►  Kernel Oops                            │
│         │                                   │                               │
│         │                                   ▼                               │
│         │                          系统崩溃 / 无法完成录音                  │
│         │                                                                   │
│         └───────────────┬──────────────────────────────────────────────────┘│
│                         │                                                   │
│                         ▼                                                   │
│               两个问题相互独立，需要分别解决                                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 9.2 调用链全景（截至当前进度）

```text
应用层: arecord
    │
    ▼
系统调用: ioctl(SNDRV_PCM_IOCTL_HW_PARAMS)
    │
    ▼
ALSA Core: snd_pcm_hw_params()
    │
    ├──► ASoC Core: soc_pcm_hw_params()
    │       │
    │       ├──► Codec DAI: acm8816_pcm_hw_params()  ← ✅ 跳过 I2C，return 0
    │       │
    │       ├──► CPU DAI: sstar_bach.c hw_params     ← ✅ 打印参数，配置 I2S 时钟
    │       │
    │       └──► Platform: sstar_pcm.c hw_params     ← ✅ 动态分配 DMA 缓冲区
    │                │
    │                └──► mhal_audio_ai_attach()     ← ⚠️ 已添加调用
    │                └──► mhal_audio_ai_open()
    │
    ▼
  trigger START
    │
    ▼
  mhal_audio_ai_start()  →  bIsAttached FALSE  ← ❌
    │
    ▼
  sstar_snd_irq_handler()  → 非法地址访问  ← ❌ Kernel Oops
```

---

## 十、后续调试计划

### 10.1 解决采样率协商问题

**目标**：让 ALSA 接受 128000 Hz，使 CPU 端采样率与 AK5538 一致。

**方案**：修改 `acm8816_dai` 的 `rates` 字段：
```c
.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_KNOT,
.rate_min = 8000,
.rate_max = 192000,
```

### 10.2 解决 Kernel Oops

**目标**：消除中断处理中的非法地址访问。

**排查方向**：
1. 确认 `mhal_audio_ai_attach` 是否真的成功
2. 检查 `mhal_audio_ai_start` 是否在 attach 完成之后才被调用
3. 检查中断是否在 DMA 上下文完全建立后才被使能
4. 在中断处理中添加 `if (bIsAttached) { ... }` 保护

### 10.3 验证物理信号（推荐每次调试前执行）

| 步骤 | 操作 | 验证内容 |
| :--- | :--- | :--- |
| 1 | `init_ak5538.sh 128` | 初始化 AK5538 |
| 2 | 示波器测 MCLK | 16.384 MHz |
| 3 | 示波器测 LRCK | 128 kHz |
| 4 | 示波器测 BCLK | 8.192 MHz |
| 5 | `arecord` 期间测 CPU BCLK/LRCK | I2S RX 是否启动 |
| 6 | 示波器测 AK5538 SDTO | 是否有连续数据 |

---

## 十一、总结：从起点到现状的完整历程

```
阶段一：TX 链路已有基础
  ├── acm8816Launch 可正常发送 PCM 数据
  ├── SDK 包含完整的 Machine/CPU DAI/Codec/PCM 驱动
  └── 仅缺少 RX/Capture 能力

阶段二：I2S0 6-Wire 配置验证
  ├── PCB → ARMTmux → PadMux DTS → Device Tree 全链已闭环
  ├── 确认 DTS 声明了 wiremode=2 (6-Wire)
  └── 待验证：Driver 是否真正将 I2S Controller 配置为 6-Wire

阶段三：ALSA 理论建设
  ├── 理解 Codec DAI .capture 如何决定 PCM Device 创建
  ├── 理解 Machine Driver 的 DTS 解析与 dai_link 填充
  ├── 理解 PCM Device 由 ASoC Core (soc_new_pcm) 创建
  └── 理解 Codec 驱动仅负责"定义能力"和"I2C 配置"

阶段四：AK5538 独立测试
  ├── 完成 BDZ → USBL 平台差异适配（I2C 总线、寄存器地址、复位逻辑）
  ├── 编写 init_ak5538.sh 一键测试脚本
  └── AK5538 硬件链路可独立验证

阶段五：集成方案设计
  ├── 修改 acm8816.c：Capture 方向跳过 I2C ✅
  ├── 修改 sstar_pcm.c：移除预分配，改为动态分配 ✅
  ├── 添加 mhal_audio_ai_attach 调⽤ ⚠️
  └── 添加 hw_free 释放逻辑 ✅

阶段六：arecord RX 调试（两轮）
  ├── 第一轮：I2C NO ACK + DMA 越界 Oops
  │   ├── 问题一：Capture 调用 ACM8816 I2C → 已解决 ✅
  │   └── 问题二：DMA 预分配 256KB < 请求 384KB → 已解决 ✅
  └── 第二轮：采样率协商 + 中断 Oops
      ├── 问题三：128k 被 ALSA 协商为 192k → ❌ 待解决
      └── 问题四：中断访问未初始化 DMA 上下文 → ❌ 待解决
```

### 当前待解决问题

| 编号 | 问题 | 根因 | 影响 |
|------|------|------|------|
| 1 | 采样率协商 128k→192k | `acm8816_dai.rates` 不含 128000 | CPU 与 AK5538 采样率不匹配 |
| 2 | 中断 Kernel Oops | `mhal_audio_ai_attach` 未完成或 DMA 上下文未初始化 | 系统崩溃，无法完成录音 |

### 核心卡点

**要使 `arecord` 能够采集 AK5538 的数据，需要解决两个相互独立的问题：**

1. **采样率协商层面**：修改 Codec DAI 的 `rates` 字段，使 ALSA 在参数协商阶段接受 128000 Hz（或其他 AK5538 支持的采样率）。
2. **DMA 中断层面**：确保 `mhal_audio_ai_attach` 成功完成、DMA 上下文完全初始化后，再使能中断并调用 `mhal_audio_ai_start`。