参考阅读：

> - [[驱动程序读取并转换 DTS 配置(acm8816)]]

我们从**用户态程序执行 `snd_pcm_writei()` 的那一刻开始**，沿着**控制流**和**数据流**两条主线，逐步追踪信号是如何从应用程序一路流淌到芯片物理管脚的。

为了便于理解，我们将一次完整的播放动作拆解为 **“配置准备阶段”** 和 **“数据传输阶段”**。

---

### 第一阶段：配置准备阶段（建立通道）

当你运行 `./acm8816Launch -f 40000 -R 96000 -b 16` 时，程序首先调用了 `snd_pcm_open`、`snd_pcm_hw_params` 和 `snd_pcm_prepare`。这一阶段**不传输音频数据**，而是**打通软件链路并配置硬件寄存器**。

#### 步骤 1：用户态打开设备（`snd_pcm_open`）
- **位置**：用户程序 `acm8816Launch.c`。
- **动作**：调用 `snd_pcm_open(&pcm_handle, "hw:0,0", SND_PCM_STREAM_PLAYBACK, 0)`。
- **流转**：`libasound` 发起 `open()` 系统调用 → **ALSA Core**（`sound/core/pcm_native.c`）根据设备号 `hw:0,0`，找到由 `sstar_asoc_card.c` 在 probe 阶段注册的 `snd_pcm` 对象。
- **结果**：创建 `struct snd_pcm_substream`（子流），并关联到 **Machine Driver** 定义的 `dai_link`。

#### 步骤 2：配置硬件参数（`snd_pcm_hw_params`）
- **位置**：用户程序调用 `snd_pcm_hw_params_set_rate(..., 96000)` 和 `snd_pcm_hw_params_set_format(..., S16_LE)`。
- **流转**：`ioctl` 进入内核 → **ALSA Core** 调用 ASoC 核心的 `soc_pcm_hw_params`（`soc-pcm.c`）。
- **核心动作（双线并行）**：
    1.  **向下通知 CPU DAI（`sstar_bach.c`）**：ASoC 调用 CPU DAI 的 `hw_params` 回调。`sstar_bach` 根据 96000Hz 采样率，**计算并配置 SSD2351 SoC 内部 I2S 控制器的分频器**（设定 BCLK 和 LRCK 的频率），并设置数据格式为 I2S 标准。
    2.  **向下通知 Codec DAI（`acm8816.c`）**：ASoC 调用 `acm8816_pcm_hw_params`。你代码中的这个函数**并没有立即发 I2C**，而是将用户传下来的 `sample_width`（16bit）暂存到 `priv` 结构体中。

#### 步骤 3：准备启动（`snd_pcm_prepare`）
- **位置**：用户程序调用 `snd_pcm_prepare()`。
- **流转**：ALSA Core → ASoC `soc_pcm_prepare`。
- **核心动作（关键的 I2C 配置来了！）**：
    - 在 `soc_pcm_prepare` 中，会触发 DAPM（动态电源管理）事件 `SND_SOC_DAPM_STREAM_START`。
    - 这会导致 ASoC 调用 Codec 驱动的 **`acm8816_set_bias_level`**，并传入 `SND_SOC_BIAS_ON`。
    - **此时，`acm8816.c` 终于动手了！** 它调用 `regmap_write`（底层是 I2C 总线驱动），通过 **I2C 引脚（SCL/SDA）** 向 ACM8816 芯片的寄存器写入配置值：
        - 写入 `ACM8816_REG_STATE_CTRL` 解除静音（Mute Off）。
        - 写入 `ACM8816_REG_I2S_DATA_FORMAT1` 设置 16bit 字长（之前暂存的值）。
        - 写入 `ACM8816_REG_STATE_CTRL` 将芯片状态切换为 `STATE_PLAY`（播放模式）。

> **至此，配置阶段结束。** 物理管脚上：CPU 端的 I2S 时钟还没开始翻转；Codec 端的 I2C 有波形，但 I2S 总线是静止的。

---

### 第二阶段：数据传输阶段（信号流动）

当用户程序调用 `snd_pcm_writei()` 时，真正的音频数据（1 和 0）开始沿着 **“DDR 内存 → DMA → I2S FIFO → I2S 管脚 → Codec 芯片”** 的路径流动。

#### 步骤 4：用户态写入数据（`snd_pcm_writei`）
- **位置**：`acm8816Launch.c` 循环调用 `snd_pcm_writei(pcm_handle, current_data, frames)`。
- **流转**：`libasound` 通过 `ioctl(SNDRV_PCM_IOCTL_WRITEI_FRAMES)` 将用户空间的 PCM 音频帧（`current_data`）拷贝到 **ALSA Core 预先分配的 Ring Buffer（环形缓冲区，位于内核空间 DDR 内存）** 中。

#### 步骤 5：隐式触发启动（Trigger START）
- 当 `snd_pcm_writei` 发现设备尚未启动时，ALSA Core 会隐式调用 `snd_pcm_trigger` 并传入 `SNDRV_PCM_TRIGGER_START`。
- **流转**：ASoC Core 调用 **`soc_pcm_trigger`**。
- **核心动作（DMA 搬移的启动命令）**：
    1.  ASoC 调用 **Platform Driver（`sstar_pcm.c`）** 的 `trigger` 回调。`sstar_pcm` **配置 DMA 控制器**：设置源地址为 ALSA Ring Buffer 的物理地址，目标地址为 SSD2351 I2S 控制器的 **TX FIFO（发送缓冲区）寄存器物理地址**，并启动 DMA 传输。
    2.  ASoC 调用 **CPU DAI Driver（`sstar_bach.c`）** 的 `trigger` 回调。`sstar_bach` **使能 I2S 硬件接口**，并开启 DMA 请求信号。

#### 步骤 6：DMA 搬运（硬件自动完成，CPU 不参与）
- **流转路径**：**DDR 内存（Ring Buffer）** → **SoC 内部 DMA 总线** → **SSD2351 I2S TX FIFO**。
- **动作**：DMA 控制器完全接管数据搬运。每当 I2S TX FIFO 快要空时，DMA 就会自动将下一块音频数据从内存塞进 FIFO。此时 **CPU 处于空闲状态（或被其他进程占用）**，不参与逐字节搬运。

#### 步骤 7：I2S 控制器发送（物理管脚电平翻转）
- **位置**：**SSD2351 的 I2S 控制器硬件模块（对应 `sstar_bach` 驱动的物理设备）**。
- **动作**：
    1.  **输出 BCLK（Bit Clock，位时钟）**：I2S 控制器内部振荡器根据步骤 2 配置的分频比，在 **物理管脚 BCLK** 上输出频率为 96000 × 16 × 2 ≈ 3.072MHz（假设双通道）的方波信号。
    2.  **输出 LRCK（Left/Right Clock，帧时钟）**：在 **物理管脚 LRCK** 上输出频率为 96kHz 的方波，高电平表示左声道，低电平表示右声道。
    3.  **输出 SDOUT（串行数据输出）**：I2S 控制器将其内部 FIFO 中的数据，按照 I2S 协议（数据在 BCLK 的下降沿变化，在上升沿被采样），**逐位（bit）** 从 **物理管脚 SDOUT** 发送出去。

> **此时，示波器可以看到：BCLK、LRCK、SDOUT 三条铜线上出现了符合 I2S 协议的数字信号波形，它们在 PCB 板层间高速行进。**

#### 步骤 8：Codec 芯片接收与转换（最终输出）
- **位置**：**ACM8816 芯片的物理引脚**（SDIN、BCLK、LRCK）。
- **动作**：
    1.  **串行接收**：ACM8816 的 I2S 接口模块在 BCLK 的上升沿锁存 SDIN 引脚上的电平，将串行比特流拼装成并行的 16-bit PCM 数据。
    2.  **DAC 转换**：将 PCM 数据送入内部 DAC（数模转换器），转换成连续的模拟电压信号。
    3.  **模拟输出**：最终，模拟信号从 ACM8816 的 **OUTP 和 OUTN（差分输出引脚）** 流出，经过外部功放、变压器，最终驱动换能器发出声波。

---

### 核心要点总结（结合你的文档）

1.  **Codec 驱动（`acm8816.c`）只负责“发 I2C 命令”**：音频数据（PCM）**从不经过** `acm8816.c` 的代码逻辑。它只是在 `prepare` 阶段通过 I2C 配置了 ACM8816 的内部寄存器，告诉芯片“准备接收”。
2.  **CPU 不搬运数据**：从步骤 5 开始，CPU 就把活交给 DMA 了。数据流的“最后一公里”搬运完全由硬件（DMA + I2S 控制器）完成。
3.  **Machine 驱动是“接线员”**：`sstar_asoc_card.c` 在开机时（probe）就把 `sstar_bach`（I2S 硬件）和 `acm8816`（Codec 硬件）绑定在一起。当 `snd_pcm_open` 时，ALSA 正是依靠这个绑定关系，才知道要把数据送给哪个 I2S 控制器的 FIFO。
4.  **物理信号的双重性**：
    - **I2C 引脚（SCL/SDA）**：在**配置阶段**有波形（写寄存器）。
    - **I2S 引脚（BCLK/LRCK/SDOUT）**：在**数据阶段**有波形（音频数据流）。
    - 这两个过程在时间上是先后发生的，并非同时，但共同完成了一次完整的播放操作。

你现在可以清楚地看到：**用户态 `acm8816Launch` 发起的 `write` 系统调用，最终转化为 SoC 引脚上 BCLK/LRCK/SDOUT 的高速电平翻转，以及 Codec 芯片管脚 OUTP/OUTN 上的模拟信号输出。** 这中间跨越了用户态、内核 VFS、ALSA Core、ASoC 框架、DMA 驱动、I2S 硬件状态机，最终在 PCB 走线上完成信号闭环。