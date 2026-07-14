结合 `soc-core.c` 和 `soc-pcm.c` 内核源码，我们现在可以**从代码层面精确锁定** PCM 设备的创建过程。

下面按照“**谁调用谁、哪个函数干了什么**”的逻辑，为你做一次完整的源码级推导。

---

# 1. 终极答案：PCM 到底由谁创建？

**PCM 设备（即 `struct snd_pcm` 和 `/dev/snd/pcmCXDXx` 节点）由 ASoC 核心框架中的 `soc_new_pcm` 函数创建**，该函数定义在 **`sound/soc/soc-pcm.c`** 中。

它不隶属于 Codec 驱动，也不隶属于 Machine 驱动，而是由 ASoC 核心在声卡绑定的最后阶段统一调用。

---

# 2. 完整函数调用链（从 Machine 驱动到 PCM 节点）

我从你的 `sstar_asoc_card.c` 的 `probe` 函数开始，梳理出这条完整的路径：

| 步骤  | 所在文件            | 关键函数                                                     | 核心作用                                                     |
| :---- | :------------------ | :----------------------------------------------------------- | :----------------------------------------------------------- |
| **1** | `sstar_asoc_card.c` | `sstar_asoc_card_probe`                                      | Machine 驱动入口，填充 `dai_link`（含 Codec 名称 `"acm8816-hifi"`），调用注册。 |
| **2** | `soc-core.c`        | `devm_snd_soc_register_card` -> `snd_soc_register_card` -> **`snd_soc_bind_card`** | 这是声卡绑定的“总指挥”。                                     |
| **3** | `soc-core.c`        | `snd_soc_bind_card` 内部调用 **`snd_soc_add_pcm_runtime`**   | 遍历 Machine 定义的 `dai_link`，为每个链路创建 `snd_soc_pcm_runtime`（简称 `rtd`）。**关键动作**：在此函数中调用 `snd_soc_find_dai` 去匹配你 Codec 注册的 `"acm8816-hifi"`。 |
| **4** | `soc-core.c`        | `snd_soc_bind_card` 内部调用 **`soc_init_pcm_runtime`**      | 在组件和 DAI 都 probe 成功后，初始化 `rtd`。**关键动作**：在此函数末尾调用 **`soc_new_pcm(rtd, num)`**。 |
| **5** | **`soc-pcm.c`**     | **`soc_new_pcm`**                                            | **这就是真正的 PCM 创建工厂！**                              |
| **6** | `soc-pcm.c`         | `soc_new_pcm` 内部调用 **`soc_get_playback_capture`**        | 读取 Codec DAI（即你的 `acm8816_dai`）中的 `.playback` 和 `.capture` 结构体，决定 `playback` 和 `capture` 变量是 1 还是 0。 |
| **7** | `soc-pcm.c`         | `soc_new_pcm` 内部调用 **`soc_create_pcm`**                  | 根据上一步的 `playback/capture` 标志，调用 **`snd_pcm_new`**（这是 ALSA 核心提供的 API）。`snd_pcm_new` 负责分配 `struct snd_pcm`、申请设备号，最终通过 `devtmpfs` 在 `/dev/snd/` 下生成 `pcmC0D0p` 和 `pcmC0D0c` 节点。 |

---

# 3. 关键源码函数详解（结合你提供的文件）

## （1）名称匹配发生在 `snd_soc_add_pcm_runtime`（`soc-core.c`）

在你的 Machine 驱动中，你将 Codec DAI 名称设为 `"acm8816-hifi"`。在 `snd_soc_add_pcm_runtime` 中，核心会调用：

```c
// soc-core.c 内部
asoc_rtd_to_codec(rtd, i) = snd_soc_find_dai(codec);
```

`snd_soc_find_dai` 会遍历全局 `component_list`（你的 `acm8816` 在 `acm8816_i2c_probe` 时通过 `devm_snd_soc_register_component` 注册进了这个链表），找到名字为 `"acm8816-hifi"` 的 DAI。**如果找不到，这里就会返回 `-EPROBE_DEFER`，PCM 根本不会创建**。

## （2）真正的创建者 `soc_new_pcm`（`soc-pcm.c`）

我们来看 `soc-pcm.c` 中的核心代码片段（你提供的文件第 2815 行附近）：

```c
int soc_new_pcm(struct snd_soc_pcm_runtime *rtd, int num)
{
    struct snd_pcm *pcm;
    int ret = 0, playback = 0, capture = 0;
    int i;

    // 1. 关键：检测 Codec DAI 是否支持播放/录音
    ret = soc_get_playback_capture(rtd, &playback, &capture);
    if (ret < 0)
        return ret;

    // 2. 创建 PCM 对象（snd_pcm_new 是 ALSA 核心 API）
    ret = soc_create_pcm(&pcm, rtd, playback, capture, num);
    if (ret < 0)
        return ret;

    // 3. 设置操作函数（此处决定了是普通 PCM 还是 DPCM）
    if (rtd->dai_link->dynamic) {
        rtd->ops.open = dpcm_fe_dai_open;
        // ...
    } else {
        rtd->ops.open = soc_pcm_open;
        // ...
    }
    // 4. 将操作函数挂载到 pcm 对象上
    if (playback)
        snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &rtd->ops);
    if (capture)
        snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &rtd->ops);
    // ...
}
```

**关键逻辑**：
- `soc_get_playback_capture` 会去轮询你的 `acm8816_dai` 结构体。如果 `.playback` 在 `acm8816.c` 中被定义了，`playback` 变量就是 `1`；如果 `.capture` 被注释掉了，`capture` 变量就是 `0`。
- `soc_create_pcm` 内部调用 `snd_pcm_new(card, name, num, playback, capture, pcm)`。注意这个函数的第 4 和第 5 个参数就是 `playback` 和 `capture` 的数量。**当你注释掉 `.capture` 后，传给 `snd_pcm_new` 的 capture 参数为 0，所以内核根本不会为捕获方向分配子流（Substream），节点自然不生成。**

---

# 4. Codec、Machine、Core 三者的关系总结

| 组件             | 源码文件                   | 核心职责       | 具体做了什么事                                               |
| :--------------- | :------------------------- | :------------- | :----------------------------------------------------------- |
| **Codec 驱动**   | `acm8816.c`                | **定义“能力”** | 1. 定义 `snd_soc_dai_driver`（含 `.playback`/`.capture` 参数）。<br>2. 调用 `devm_snd_soc_register_component`，将自身能力注册到内核的 `component_list` 链表中。<br>**它不调用 `snd_pcm_new`。** |
| **Machine 驱动** | `sstar_asoc_card.c`        | **定义“连接”** | 1. 定义 `snd_soc_dai_link`，指定 Codec DAI 名字为 `"acm8816-hifi"`。<br>2. 调用 `devm_snd_soc_register_card`，触发声卡绑定流程。<br>**它不调用 `snd_pcm_new`。** |
| **ASoC 核心**    | `soc-core.c` / `soc-pcm.c` | **执行“创建”** | 1. 在 `snd_soc_bind_card` 中执行 `snd_soc_add_pcm_runtime`（匹配名字）。<br>2. 在 `soc_init_pcm_runtime` 中调用 **`soc_new_pcm`**。<br>3. `soc_new_pcm` 读取 Codec 定义的参数，调用 **`snd_pcm_new`** 完成最终创建。<br>**PCM 设备节点在此诞生。** |

---

# 5. 针对你之前实验现象的源码级解释

**现象**：注释掉 `acm8816_dai` 的 `.capture` 后，`pcmC0D0c` 消失。

**源码根因**：
在 `soc-pcm.c` 的 `soc_get_playback_capture` 函数中，会遍历 Codec DAI：

```c
if (snd_soc_dai_stream_valid(codec_dai, SNDRV_PCM_STREAM_PLAYBACK) &&
    snd_soc_dai_stream_valid(cpu_dai, cpu_playback))
    *playback = 1;
if (snd_soc_dai_stream_valid(codec_dai, SNDRV_PCM_STREAM_CAPTURE) &&
    snd_soc_dai_stream_valid(cpu_dai, cpu_capture))
    *capture = 1;
```

因为 `snd_soc_dai_stream_valid` 检查到你的 Codec DAI 中 `capture` 结构体为空，所以 `capture` 变量被赋值为 `0`。随后 `snd_pcm_new(..., playback, capture, ...)` 收到 `capture = 0`，于是这个 PCM 实例只有 Playback 方向，没有 Capture 方向。设备节点 `pcmC0D0c` 自然不会被 `devtmpfs` 生成。

---

**一句话最终结论**：**PCM 设备由 ASoC 核心中的 `soc_new_pcm`（位于 `soc-pcm.c`）创建。** 它依赖于 Codec 驱动提供的 `playback/capture` 结构体作为“施工图纸”，依赖于 Machine 驱动的 `dai_link` 作为“施工指令”。你注释 `capture` 的本质，是修改了传给 `snd_pcm_new` 的“通道数量参数”。