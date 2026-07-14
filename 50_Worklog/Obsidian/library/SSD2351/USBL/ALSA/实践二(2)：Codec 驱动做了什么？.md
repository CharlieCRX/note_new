**Codec 驱动本身并不直接“创建”PCM 设备（即 `/dev/snd/pcmCXDXx` 节点和内核中的 `struct snd_pcm` 实例）。**

在 ASoC（ALSA System on Chip）框架中，存在严格的**职责分离**。结合你的 `acm8816.c` 代码，我们可以清晰地看到这一点：

- **Codec 驱动（你的代码）**：扮演 **“能力定义者”** 的角色。
- **Machine 驱动 + ASoC 核心**：扮演 **“实际构建者”** 的角色。

------

# Codec 驱动做了什么？（定义蓝图）

在你的 `acm8816.c` 中，PCM 相关的操作停留在**定义**层面：

```C
/* DAI操作结构体 */
static const struct snd_soc_dai_ops acm8816_dai_ops = {
    .set_sysclk = acm8816_set_dai_sysclk,
    .set_fmt = acm8816_set_dai_fmt,
    .hw_params = acm8816_pcm_hw_params,
    .startup = acm8816_pcm_startup,
    .shutdown = acm8816_pcm_shutdown,
};

/* DAI驱动定义 - 支持播放和录音 */
static struct snd_soc_dai_driver acm8816_dai = {
    .name = "acm8816-hifi",
    .playback = {
        .channels_min = 1,
        .channels_max = 1,
        .rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
        .formats = SNDRV_PCM_FMTBIT_S16_LE  | SNDRV_PCM_FMTBIT_S32_LE,
    },

    .capture = {
        .channels_min = 1,
        .channels_max = 1,
        .rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
        .formats = SNDRV_PCM_FMTBIT_S16_LE  | SNDRV_PCM_FMTBIT_S32_LE,
    },
    .ops = &acm8816_dai_ops,
};
```

- 定义了 `snd_soc_dai_driver acm8816_dai`，说明了硬件支持哪些速率、格式和数据流向（`.playback` / `.capture`）。
- 定义了 `snd_soc_dai_ops acm8816_dai_ops`，提供了操作硬件的回调函数（如 `hw_params`）。

**最关键的一步**是在函数`acm8816_i2c_probe()`中：

```C
ret = devm_snd_soc_register_component(&i2c->dev, &acm8816_component_driver, &acm8816_dai, 1);
```



这个函数的作用是**向 ASoC 核心注册一个“组件”（Component）**，并告知核心：“我这里有一个名叫 `acm8816-hifi` 的 DAI，它具备这些能力。”

**但是，`devm_snd_soc_register_component` 并不会调用 `snd_pcm_new`，也不会在 `/dev/snd/` 下创建设备节点。** 它只是把这份“设计图纸”交给了系统。