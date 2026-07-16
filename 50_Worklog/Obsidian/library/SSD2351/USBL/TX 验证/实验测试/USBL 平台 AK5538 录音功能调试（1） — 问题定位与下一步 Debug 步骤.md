| 文档版本     | V1.0                                        |
| :------- | :------------------------------------------ |
| **日期**   | 2026-07-15                                  |
| **适用平台** | USBL（SSD2351 + FPGA + ACM8816 + AK5538）     |
| **当前状态** | 已消除 I2C 错误，流程推进至 DMA 分配阶段，但 Kernel Oops 仍存在 |
| **关联文档** | 《USBL 平台 AK5538 录音功能集成实践方案》                 |


## 一、当前问题现象

### 1.1 执行命令

```bash
arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 1 /dev/null
```

### 1.2 完整日志

```text
enter ai power manage
sstar-bach 1f2a0400.bach1: mclk = 12288000
sstar-bach 1f2a0400.bach1: codec_dai.name = acm8816-hifi
acm8816 1-0040: Enter acm8816_set_dai_sysclk - clk_id: 0, freq: 12288000 Hz, dir: 1
sstar-bach 1f2a0400.bach1: Activating I2S_TX_A path for ACM8816
acm8816 1-0040: Enter acm8816_pcm_startup - stream: Capture
acm8816 1-0040: Enter acm8816_pcm_hw_params - rate: 192000 Hz, channels: 1, format: 0xa, sample width: 32 bits
acm8816 1-0040: Capture: skip I2C config, AK5538 already configured by script   ← ✅ I2C 错误已消除
sstar-bach 1f2a0400.bach1: rate[192000] chanl[1], bufferbyte[384000], buffersize[96000], periodbyte[96000], periodsize[24000], period[4] wdma[0] bitwidth[32]
Warning: rate is Unable to handle kernel paging request at virtual address ffffffc009b5d000   ← ❌ Kernel Oops
...
Call trace:
 __memset+0x16c/0x188
 snd_pcm_ioctl+0x528/0xf78
 ...
acm8816 1-0040: Enter acm8816_pcm_shutdown - stream: Capture
sstar-bach 1f2a0400.bach1: [sstar_pcm_close][687] wdma [0]
not accurate (requested = 128000Hz, got = 192000Hz)
         please, try the plug plugin 
Segmentation fault
```


## 二、日志分析与当前进度

### 2.1 已确认成功的事项 ✅

| 检查项 | 日志证据 | 状态 |
| :--- | :--- | :--- |
| ACM8816 驱动被调用 | `Enter acm8816_pcm_hw_params` | ✅ |
| Capture 方向被正确识别 | `stream: Capture` | ✅ |
| I2C 操作被跳过 | `Capture: skip I2C config` | ✅ |
| **I2C 错误已消除** | **没有 `i2c-1 no ack`** | ✅ |
| CPU DAI 被调用 | `sstar-bach` 打印参数 | ✅ |

### 2.2 当前卡住的位置 ❌

```text
Unable to handle kernel paging request at virtual address ffffffc009b5d000
pc : __memset+0x16c/0x188
lr : snd_pcm_hw_params+0x268/0x33c
```

**崩溃位置**：`snd_pcm_hw_params`（ALSA Core）→ `__memset` 清零 DMA 缓冲区时，访问了非法虚拟地址 `0xffffffc009b5d000`。

### 2.3 崩溃的直接原因

| 原因分析 | 说明 |
| :--- | :--- |
| **非法地址** | `0xffffffc009b5d000` 是一个内核虚拟地址，但页表显示 `pte=0000000000000000`，表示该地址**未映射到物理内存** |
| **触发操作** | ALSA Core 在 `snd_pcm_hw_params` 中调用 `__memset` 对 `runtime->dma_area` 进行清零 |
| **根本原因** | `runtime->dma_area` 指向了**无效或未映射的内存地址** |

### 2.4 结论

**你的 `acm8816.c` 修改是成功的。** I2C 障碍已被清除，录音流程已经推进到了 **DMA 缓冲区分配与映射** 阶段。

当前的问题 **与 Codec 驱动（`acm8816.c`）无关**，而是 **Platform 驱动（`sstar_pcm.c`）** 在分配 DMA 缓冲区时返回了无效地址。


## 三、调用链分析：当前执行到了哪一步？

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
    │       ├──► Codec DAI: acm8816_pcm_hw_params()   ← ✅ 你修改了这里，跳过 I2C
    │       │
    │       ├──► CPU DAI: sstar_bach.c 的 hw_params   ← ✅ 打印了参数，配置 I2S 时钟
    │       │
    │       └──► Platform: sstar_pcm.c 的 hw_params   ← ❌ DMA 缓冲区分配出问题
    │
    ▼
ALSA Core: __memset(runtime->dma_area, 0, ...)   ← ❌ 崩溃在这里
```

**关键结论**：程序已经成功走过了 Codec DAI 和 CPU DAI，但在 Platform DAI 分配完 DMA 缓冲区后、ALSA Core 尝试清零缓冲区时崩溃。


## 四、问题根因定位

### 4.1 核心问题

**`sstar_pcm.c` 在 `hw_params` 阶段分配的 DMA 缓冲区地址无效。**

可能的子问题：

| 序号 | 可能原因 | 说明 |
| :--- | :--- | :--- |
| 1 | **DMA 分配函数返回 NULL** | `alloc_dmem()` 或 `msys_request_dmem()` 分配失败，返回 `NULL` |
| 2 | **`runtime->dma_area` 未赋值** | `hw_params` 中没有正确设置 `runtime->dma_area = buf->area` |
| 3 | **虚拟地址未映射** | 返回的 `kvirt` 地址虽然非零，但未真正映射到物理内存 |
| 4 | **内存池耗尽** | 系统 DMA 内存池已被其他模块耗尽 |

### 4.2 从崩溃地址分析

```text
x0 : ffffffc009b1d000   ← __memset 的目标地址（即 runtime->dma_area）
x2 : 000000000001dfc0   ← 要清零的字节数（约 122,816 字节）

目标地址 = 0xffffffc009b1d000
崩溃地址 = 0xffffffc009b5d000  ← 访问到这里时触发缺页
```

两者的差值约 0x40000（256KB），这表明 `__memset` 在对一个 **256KB 的 DMA 缓冲区**进行清零时，访问到了缓冲区**末尾附近**的非法地址。这强烈暗示 **DMA 缓冲区本身没有完整映射**。


## 五、下一步 Debug 步骤

### 5.1 定位 `sstar_pcm.c` 中的 DMA 分配代码

首先，找到 `sstar_pcm.c` 中负责 DMA 缓冲区分配的函数。

通常在 `hw_params` 回调中，会有类似以下逻辑：

```c
static int sstar_pcm_hw_params(struct snd_pcm_substream *substream,
                               struct snd_pcm_hw_params *params,
                               struct snd_soc_dai *dai)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    struct sstar_pcm_stream *ss_pcm_stream = runtime->private_data;
    struct sstar_pcm_buffer *buf;
    int ret;

    // 分配 DMA 缓冲区
    buf = alloc_dmem(size);  // 或者 msys_request_dmem()
    if (!buf) {
        dev_err(...);
        return -ENOMEM;
    }

    runtime->dma_area = buf->area;      // ← 关键：dma_area 必须被赋值
    runtime->dma_addr = buf->phy_addr;  // ← 关键：dma_addr 必须被赋值
    runtime->dma_bytes = size;          // ← 关键：dma_bytes 必须被赋值

    return 0;
}
```

### 5.2 添加调试打印

在 `sstar_pcm.c` 的 `hw_params` 函数中添加打印：

```c
dev_info(dev, "sstar_pcm_hw_params: dma_area = %p, dma_addr = %pad, dma_bytes = %zu\n",
         runtime->dma_area, &runtime->dma_addr, runtime->dma_bytes);
```

**重点检查**：
- `runtime->dma_area` 是否为 `NULL` 或 `0x0`
- `runtime->dma_bytes` 是否与预期一致
- `runtime->dma_addr` 是否为有效的物理地址

### 5.3 检查 `alloc_dmem()` 或 `msys_request_dmem()` 实现

找到 DMA 分配函数的具体实现，检查：

```c
// 示例：检查 msys_request_dmem
buf->area = msys_request_dmem(size);  // 返回 kvirt 地址
buf->phy_addr = msys_get_phy_addr(buf->area);  // 获取物理地址
```

**需要确认**：
1. `msys_request_dmem()` 返回的 `kvirt` 地址是否真的是有效的内核虚拟地址
2. 该地址是否已经通过 `ioremap` 或 `dma_alloc_coherent` 正确映射
3. 物理地址是否与虚拟地址一一对应

### 5.4 使用标准 API 替代（推荐）

如果当前使用的是自定义内存分配，建议改用 ALSA/内核标准 API：

```c
// 方案 A：使用 ALSA 标准 API（推荐）
ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
if (ret < 0) {
    dev_err(...);
    return ret;
}
// snd_pcm_lib_malloc_pages 会自动设置 runtime->dma_area、dma_addr、dma_bytes
```

```c
// 方案 B：使用内核 DMA API
buf->area = dma_alloc_coherent(dev, size, &buf->phy_addr, GFP_KERNEL);
if (!buf->area) {
    return -ENOMEM;
}
runtime->dma_area = buf->area;
runtime->dma_addr = buf->phy_addr;
runtime->dma_bytes = size;
```

### 5.5 验证修改

修改后，重新编译并执行测试：

```bash
# 清空日志
dmesg -c

# 执行录音
arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 1 /dev/null

# 查看日志
dmesg | tail -30
```

**预期结果**：
- `runtime->dma_area` 打印出有效的虚拟地址（非 NULL）
- 不再出现 `Unable to handle kernel paging request`
- `arecord` 正常完成，生成有效录音文件


## 六、调试流程图

```text
┌─────────────────────────────────────────────────────────────────────┐
│ 第一步：确认当前卡在哪一步                                         │
│   ✅ I2C 已绕过 → ✅ CPU DAI 已执行 → ❌ DMA 分配失败              │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│ 第二步：定位 sstar_pcm.c 中的 hw_params 函数                       │
│   - 找到 struct snd_soc_ops 或 struct snd_pcm_ops 的定义           │
│   - 找到 .hw_params = xxx 对应的函数实现                           │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│ 第三步：添加调试打印，确认关键变量                                  │
│   - runtime->dma_area 是否为 NULL？                                │
│   - runtime->dma_addr 是否有效？                                   │
│   - runtime->dma_bytes 是否正确？                                  │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│ 第四步：检查 DMA 分配函数（alloc_dmem / msys_request_dmem）        │
│   - 是否成功返回内存？                                             │
│   - 返回的 kvirt 是否可访问？                                      │
│   - 物理地址与虚拟地址是否正确对应？                               │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│ 第五步：改用标准 API 或修复分配逻辑                                │
│   - 方案 A: snd_pcm_lib_malloc_pages()                            │
│   - 方案 B: dma_alloc_coherent()                                  │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│ 第六步：重新编译测试                                               │
│   - 观察新日志确认 dma_area 已生效                                 │
│   - 不再出现 Kernel Oops                                           │
│   - arecord 生成有效录音文件                                       │
└─────────────────────────────────────────────────────────────────────┘
```


## 七、需要关注的具体代码位置

| 序号 | 文件 | 需要查找的内容 |
| :--- | :--- | :--- |
| 1 | `sstar_pcm.c` | `hw_params` 回调函数实现 |
| 2 | `sstar_pcm.c` 或 `sstar_dmem.c` | `alloc_dmem()` 或 `msys_request_dmem()` 函数 |
| 3 | `sstar_pcm.h` 或相关头文件 | `struct sstar_pcm_stream` 或 `struct sstar_pcm_buffer` 定义 |
| 4 | ALSA 框架文档 | `snd_pcm_lib_malloc_pages` API 用法 |


## 八、成功标志

完成 Debug 后，应满足以下所有条件：

| # | 检查项 | 状态 |
| :--- | :--- | :--- |
| 1 | 内核日志显示 `dma_area = <有效地址>` | ☐ |
| 2 | `arecord` 不再报 `i2c-1 no ack` | ☐ |
| 3 | `arecord` 不再发生 Kernel Oops | ☐ |
| 4 | 生成 `test.wav` 文件大小 > 0 | ☐ |
| 5 | 播放录音文件有正常音频数据（非全零） | ☐ |


## 九、总结

| 问题 | 答案 |
| :--- | :--- |
| **修改后卡在哪一步？** | 卡在 **Platform 驱动（`sstar_pcm.c`）分配 DMA 缓冲区** 的阶段 |
| **崩溃的直接原因？** | `runtime->dma_area` 指向了 **未映射的非法虚拟地址** |
| **与 `acm8816.c` 修改有关吗？** | **无关。** 你的修改是成功的，I2C 错误已消除 |
| **下一步应该找哪里？** | **`sstar_pcm.c` 的 `hw_params` 函数和 DMA 内存分配逻辑** |
| **优先尝试什么方案？** | 使用 `snd_pcm_lib_malloc_pages()` 替代自定义内存分配 |

> 本文档基于 USBL 平台 AK5538 录音调试的实际日志整理，执行完成后请更新各检查项状态。