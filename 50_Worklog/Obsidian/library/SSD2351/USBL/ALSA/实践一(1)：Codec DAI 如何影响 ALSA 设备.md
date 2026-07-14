
# 实验目的

理解：

> Codec Driver 中的 DAI 描述并不仅仅是描述 Codec，而是决定了 ALSA 最终向用户空间暴露哪些能力。

这是理解 ALSA 的第一步。

------

## 实验内容

原始 Codec DAI：（定义在`kernel\sound\soc\codecs\acm8816.c`）

```c
static struct snd_soc_dai_driver acm8816_dai = {

    .name = "acm8816-hifi",

    .playback = {
        ...
    },

    .capture = {
        ...
    },

    .ops = &acm8816_dai_ops,
};
```

实验修改：

将

```c
.capture = {
    ...
},
```

全部注释掉。重新编译 Kernel 后，重新烧录。

启动开发板。

执行：

```bash
aplay -l
```

得到：

```text
**** List of PLAYBACK Hardware Devices ****

card0

device0

HiFi PCM acm8816-hifi-0
```

说明：

播放设备依旧存在。

然后：

```bash
arecord -l
```

得到：

```text
**** List of CAPTURE Hardware Devices ****
```

没有任何设备。

------

# 实验现象分析

很多初学者会认为：

> Codec Driver 删除 Capture，所以 Codec 不能录音。

实际上：

**真正发生的事情比这更重要。**

发生的是：

Codec Driver 向 ASoC 注册 DAI 时：

原来告诉 Framework：

```text
我支持：

Playback

Capture
```

现在告诉 Framework：

```text
我只支持：

Playback
```

ASoC Framework 根据这个能力重新构建：

Machine Driver

↓

DAI Link

↓

PCM Device

由于：

Capture 不存在。

于是：

整个 PCM Device 不再具有 Capture 能力。

因此：

```bash
arecord -l
```

看不到任何设备。

注意：

这里并不是 arecord 检测不到 Codec。

而是：

Linux 根本没有创建 Capture PCM Device。

------

# 本实验得到的重要结论

得到第一个结论：

> PCM Device 并不是用户程序创建的。

也不是 Codec Driver 创建的。

而是 ASoC Framework 根据 CPU DAI 和 Codec DAI 共同生成。

因此：

Codec DAI 的能力发生变化。

整个 PCM Device 也发生变化。

------

# 本实验验证的问题

完成本实验后，应能够回答：

为什么：

```bash
arecord -l
```

没有设备？

正确答案：

- 不是 Codec 没了。

- 不是 I2S RX 坏了。

- 不是 DMA 坏了。

而是：

ASoC Framework 在创建 PCM Device 时，发现 Codec DAI 没有 Capture。

因此没有创建 Capture PCM。