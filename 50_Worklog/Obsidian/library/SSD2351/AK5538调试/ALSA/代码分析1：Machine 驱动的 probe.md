# 0. 为什么需要 Machine 驱动的 `probe`？

想象一下：

-   **Codec 驱动（`acm8816.c`）** 就像一本“芯片使用说明书”，它告诉系统：“我能做 48k/16bit 的播放和录音，操作我的方法是 I2C 写寄存器”。
-   **CPU（SoC）内部的 I2S 控制器驱动** 就像“数据搬运工”，它告诉系统：“我负责把内存里的音频数据通过 I2S 总线发出去”。

**问题来了**：说明书（Codec）和搬运工（CPU I2S）都是独立的模块，系统怎么知道**具体把哪个搬运工（CPU I2S 端口 0）和哪本说明书（Codec 芯片）连接在一起**？

答案就在 **`sstar_asoc_card_probe`** 中。

> **`sstar_asoc_card_probe` 的核心职责**：它是“**焊接工**”和“**接线员**”。它读取电路板（Device Tree，即设备树）的硬件连线图，告诉 ASoC 核心：“**我的这块电路板上，CPU 的 I2S 端口 A，物理上连接着 ACM8816 这颗 Codec 芯片。请你们俩合作，形成一个音频通路。**”

如果没有这个 `probe` 函数，ASoC 核心手里虽然有 CPU 驱动和 Codec 驱动，但它们就像两个没有电话线的电话机，永远无法建立通话（也就无法生成 PCM 设备节点）。

---

# 1. 逐步拆解 `sstar_asoc_card_probe` 的源码逻辑

现在，我们一行一行地看这个函数在具体做什么。

## 第一步：确定硬件版本（第 110~115 行）
```c
of_id = of_match_device(sstar_asoc_card_dt_ids, &pdev->dev);
if (!of_id) return -EINVAL;
index = (kernel_ulong_t)of_id->data;
```
- **功能**：检查设备树（DTS）中的 `compatible` 属性。比如，如果你的 DTS 写的是 `"sstar, asoc-card"`，它就取索引 `0`；如果是 `"sstar, asoc2-card"`，取索引 `1`。
- **为什么做**：SigmaStar 这个 Machine 驱动可能要兼容好几种不同的开发板布局，通过这个索引，后面会选择不同的 `dai_link` 数组（比如有的板子只接了 1 个 Codec，有的接了 5 个）。

## 第二步：找到 CPU 侧的 I2S 控制器（第 117~126 行）
```c
np_tmp = of_find_node_by_name(np, "sstar-audio-card,cpu");
np_cpu_d0 = of_parse_phandle(np_tmp, "sound-dai", 0);
```
- **功能**：去 DTS 里找名为 `"sstar-audio-card,cpu"` 的节点，然后解析里面的 `sound-dai` 属性，找到对应的 CPU DAI（Digital Audio Interface，即 I2S 控制器）的设备树节点指针。
- **为什么做**：Machine 驱动必须明确知道**数据是从哪个 CPU 的哪个引脚（I2S0 还是 I2S1）发出去的**。这是数据通路的起点。

## 第三步：找到 Codec 侧的音频芯片（第 128~158 行）—— **这是与你 `acm8816` 连接最关键的一步！**
```c
np_tmp = of_find_node_by_name(np, "sstar-audio-card,codec");
for (i = 0;; i++) {
    np_codec = of_parse_phandle(np_tmp, "sound-dai", i);
    // ...
    codec_name = devm_kzalloc(...);
    if (of_property_read_string(np_codec, "codec-name", &codec_name) != 0) {
        /* 如果 DTS 没指定 codec-name，就用兼容性字符串硬编码 */
        if (strcmp(compatible, "acm8816") == 0) {
            strncpy(codec_name, "acm8816-hifi", CODEC_NAME_BYTES - 1);
        }
    }
    // 关键赋值！
    card->dai_link[i].codecs->of_node  = np_codec;
    card->dai_link[i].codecs->dai_name = codec_name; // 这里变成了 "acm8816-hifi"
}
```
- **功能**：遍历 DTS 中定义的所有 Codec（可能不止一个）。对于每一个 Codec，它去读取名字。特别地，如果 DTS 没写名字，但兼容性是 `"acm8816"`，它就**手动**把 `dai_name` 设置为 `"acm8816-hifi"`。
- **为什么做**：
    1.  **物理绑定**：告诉系统，这根数据线的终点是这颗 Codec 芯片。
    2.  **名字匹配（极其重要）**：你之前在 `acm8816.c` 中写的 `static struct snd_soc_dai_driver acm8816_dai = { .name = "acm8816-hifi" ... }`，和这里赋值的 `"acm8816-hifi"` **必须一字不差**。这就像寄快递，这里的“收件人名字”必须和 Codec 驱动注册时贴的“标签”一致，ASoC 核心才能把快递（音频数据）正确送达。

## 第四步：搭建“数据链路”结构体（第 159~163 行）
```c
card->dai_link[i].cpus->of_node      = np_cpu_d0;
card->dai_link[i].platforms->of_node = np_cpu_d0;
card->num_links                      = i + 1;
```
- **功能**：将前面找到的 CPU 节点和 Codec 节点，填充进 `card` 的 `dai_link` 数组里。`platforms` 通常也指向 CPU，因为 DMA 引擎通常挂在 CPU 侧。
- **为什么做**：`dai_link` 是 ASoC 核心最重要的数据结构。它就像一个“婚介所登记表”，左边写着 CPU（男方），右边写着 Codec（女方）。核心在后续的 `soc_bind_card` 中，就是**只认这张表**来举办婚礼（创建 PCM）的。

## 第五步：注册声卡（最终一击！）（第 172~180 行）
```c
card->dev = dev;
ret = devm_snd_soc_register_card(&pdev->dev, card);
```
- **功能**：调用 ASoC 核心提供的标准注册接口。
- **为什么做**：**到这里，Machine 驱动的使命就完成了！** 它把填好的“婚介表”（`card` 结构体）郑重地交到了 ASoC 核心（相当于民政局）的手里。

---

# 2. 总结：`sstar_asoc_card_probe` 到底做了什么，以及为什么必须做？

| 做了什么（功能）                            | 为什么必须做（意义）                                         |
| :------------------------------------------ | :----------------------------------------------------------- |
| **解析 DTS**                                | 硬件接线是千变万化的。不写死代码，通过 DTS 告诉驱动“CPU 的 I2C 通道几连了哪个 Codec”，让同一个驱动可以在不同主板上复用。 |
| **硬编码 `codec_name` 为 `"acm8816-hifi"`** | 完成 **“名称匹配”**。这是连接你的 `acm8816.c` 驱动和本 Machine 驱动的**唯一身份证**。名字对不上，Core 就无法把两者关联起来，`soc_new_pcm` 就永远不会被调用。 |
| **填充 `dai_link`**                         | 告诉 ASoC 核心 **“谁是数据源（CPU），谁是数据终点（Codec）”**。核心只认这个结构体来分配 DMA 通道和配置 I2S 引脚。 |
| **调用 `devm_snd_soc_register_card`**       | **触发 PCM 创建的起点！** 正是这个函数调用，才让 ASoC 核心进入 `soc-core.c`，最终一步步走到 `soc-pcm.c` 里的 `soc_new_pcm`。如果没有这一步，你的 Codec 驱动写得再完美，`/dev/snd/` 下也永远不会有任何 PCM 节点。 |

**一句话记住它**：`sstar_asoc_card_probe` **不生产 PCM**，它只是 **PCM 创建的“报幕员”**。它负责把“谁（CPU）和谁（Codec）要搭档”这个信息准确地告诉 ASoC 核心。后面的创建流程，我们在下一次再顺着 `devm_snd_soc_register_card` 继续慢慢往下讲。