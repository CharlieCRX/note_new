| 文档版本     | V1.0                                                                    |
| :------- | :---------------------------------------------------------------------- |
| **日期**   | 2026-07-15                                                              |
| **关联问题** | `arecord` 执行时发生 `Unable to handle kernel paging request`，崩溃在 `__memset` |


## 一、问题回顾

执行 `arecord` 时，内核崩溃：

```text
Unable to handle kernel paging request at virtual address ffffffc009b5d000
pc : __memset+0x16c/0x188
lr : snd_pcm_hw_params+0x268/0x33c
```

崩溃发生在 ALSA Core 对 DMA 缓冲区执行 `memset` 清零操作时，访问了非法虚拟地址。

## 二、代码分析

### 2.1 DMA 缓冲区分配（`sstar_pcm_probe`）

在 `sstar_pcm_probe` 中，驱动程序预先分配了固定大小的 DMA 缓冲区：

```c
static int sstar_pcm_probe(struct snd_soc_component *component)
{
    // ...
    int size_max = 512 * 512; // = 262144 字节 (256KB)

    // 为录音 (rx) 分配缓冲区
    buf = &ss_pcm->rx.buf;
    buf->area = alloc_dmem(name, PAGE_ALIGN(size_max), (dma_addr_t *)(&buf->addr));
    buf->bytes = PAGE_ALIGN(size_max);   // 实际分配大小：256KB

    // 为播放 (tx) 分配缓冲区，如果 rdma == 0 则分配 1MB，否则也是 256KB
    if (ss_pcm->rdma == 0) {
        size_max = 1024 * 1024;          // 1MB
    }
    buf->area = alloc_dmem(name, PAGE_ALIGN(size_max), ...);
    buf->bytes = PAGE_ALIGN(size_max);
}
```

**关键点**：
- 录音（Capture）方向固定分配 **256KB**。
- 播放方向除 `rdma==0` 外也分配 256KB。

### 2.2 `hw_params` 中的赋值（`sstar_pcm_hw_params`）

当用户执行 `arecord` 时，调用 `sstar_pcm_hw_params`：

```c
static int sstar_pcm_hw_params(..., struct snd_pcm_hw_params *hw_params)
{
    // ...
    ss_pcm_stream->pcm.u32BufferSize = params_buffer_bytes(hw_params);  // 请求的缓冲区大小
    // ...
    substream->runtime->dma_bytes = ss_pcm_stream->pcm.u32BufferSize;   // 设置为请求大小
    substream->runtime->dma_addr  = ss_pcm_stream->buf.addr;            // 物理地址（正确）
    substream->runtime->dma_area  = ss_pcm_stream->buf.area;            // 虚拟地址（256KB 区域）
    substream->dma_buffer.bytes   = params_buffer_bytes(hw_params);     // 请求大小
}
```

**关键问题**：
- `runtime->dma_bytes` 被设置为 `params_buffer_bytes(hw_params)`（即用户请求的缓冲区大小）。
- 但 `runtime->dma_area` 指向的虚拟内存区域只有 **256KB**（由 `probe` 分配）。
- 如果请求的缓冲区大小 **大于 256KB**，则 `runtime->dma_bytes` 会超出实际映射的内存范围。

### 2.3 日志中的实际参数

```text
sstar-bach: rate[192000] chanl[1], bufferbyte[384000], buffersize[96000], periodbyte[96000], periodsize[24000], period[4] wdma[0] bitwidth[32]
```

- `bufferbyte[384000]` = 384,000 字节 ≈ **375 KB**。
- 请求的缓冲区大小 **375KB > 256KB**（预分配大小）。

### 2.4 崩溃原因

ALSA Core 在 `snd_pcm_hw_params` 中会对 `runtime->dma_area` 执行 `memset` 清零，大小由 `runtime->dma_bytes` 决定：

```c
// ALSA Core (sound/core/pcm_native.c) 中类似逻辑
memset(runtime->dma_area, 0, runtime->dma_bytes);
```

由于 `dma_area` 只有 256KB 映射，但 `dma_bytes` 为 375KB，`memset` 会访问 256KB 之后的地址（`0xffffffc009b5d000` 附近），该地址未映射 → 缺页异常。

**崩溃地址验证**：
- `x0` (`dma_area`) = `0xffffffc009b1d000`
- `x2` (`dma_bytes`) = `0x1dfc0` ≈ 122,816 字节（这是 `period` 大小？实际上 `bufferbyte` 384000，但 `memset` 可能先清 `period` 部分，但仍超过实际映射范围）

无论如何，请求大小超过分配大小是根本原因。

## 三、问题根本原因总结

| 问题 | 说明 |
| :--- | :--- |
| **预分配大小固定** | `sstar_pcm_probe` 中为录音固定分配 256KB，播放除 rdma0 外也 256KB |
| **请求大小可变** | `sstar_pcm_hw_params` 将 `runtime->dma_bytes` 设置为用户请求的缓冲区大小，该大小可能超过预分配大小 |
| **大小不匹配** | 导致 ALSA Core 对未映射的内存执行 `memset`，触发缺页异常 |

## 四、解决方案

### 4.1 方案一：动态分配 DMA 缓冲区（推荐）

在 `sstar_pcm_hw_params` 中，根据请求的缓冲区大小动态分配内存，而非在 `probe` 中固定分配。

**修改步骤**：

1. **移除 `probe` 中的预分配**（或保留但仅作为初始值，但必须确保 `hw_params` 能重新分配）。

2. **在 `hw_params` 中添加分配逻辑**：

```c
static int sstar_pcm_hw_params(..., struct snd_pcm_hw_params *hw_params)
{
    // ...
    size_t size = params_buffer_bytes(hw_params);
    // 如果当前缓冲区大小不足，释放旧缓冲区并重新分配
    if (ss_pcm_stream->buf.bytes < size) {
        if (ss_pcm_stream->buf.area) {
            free_dmem(..., ss_pcm_stream->buf.area, ...);
            ss_pcm_stream->buf.area = NULL;
        }
        ss_pcm_stream->buf.area = alloc_dmem(name, PAGE_ALIGN(size), &ss_pcm_stream->buf.addr);
        if (!ss_pcm_stream->buf.area) {
            return -ENOMEM;
        }
        ss_pcm_stream->buf.bytes = PAGE_ALIGN(size);
    }
    // 然后赋值给 runtime
    substream->runtime->dma_bytes = size;  // 现在大小匹配
    substream->runtime->dma_area = ss_pcm_stream->buf.area;
    // ...
}
```

3. **在 `hw_free` 或 `close` 中释放缓冲区**（或保留到下次重用）。

**优点**：完全适配请求大小，内存利用率高，无越界风险。

### 4.2 方案二：增大固定缓冲区大小

将 `size_max` 修改为一个足够大的值，例如 `1024 * 1024`（1MB），以覆盖所有可能的请求。

**修改**：

```c
int size_max = 1024 * 1024; // 1MB
```

**缺点**：如果请求超过 1MB 仍会失败；内存浪费（即使小请求也占用 1MB）。

### 4.3 方案三：使用 ALSA 标准 API `snd_pcm_lib_malloc_pages`

这是最标准的方式，完全由 ALSA 框架管理缓冲区：

```c
ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
if (ret < 0) return ret;
// 之后 runtime->dma_area、dma_addr、dma_bytes 自动设置
```

但需要确保 `sstar_pcm_probe` 中没有预分配冲突，且 `sstar_pcm_hw_free` 中调用 `snd_pcm_lib_free_pages`。

**优点**：框架保证正确性，无需手动管理。

## 五、推荐实施路径

1. **短期验证**：修改 `size_max = 1024 * 1024`，重新编译测试，确认崩溃是否消失。
2. **长期方案**：采用方案一或方案三，实现动态分配，提高代码健壮性。

## 六、验证方法

修改后重新编译内核或模块，执行：

```bash
arecord -D hw:0,0 -r 128000 -c 1 -f S32_LE -d 1 /dev/null
```

观察：
- 不再出现 `Unable to handle kernel paging request`
- 日志中 `dma_bytes` 与分配大小一致

## 七、结论

当前崩溃的直接原因是 **DMA 缓冲区预分配大小（256KB）小于 `hw_params` 请求的缓冲区大小（384KB）**，导致越界访问。解决此问题即可推进录音功能至下一步数据验证阶段。