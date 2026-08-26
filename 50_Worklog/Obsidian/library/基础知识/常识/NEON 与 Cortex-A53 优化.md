**核心概念**：NEON 是 ARM 平台的 SIMD 指令集，可以让一条指令同时处理多组数据，适合 FFT、DSP、图像、音频等计算。

目标板信息检查：

```
cat /proc/cpuinfo
```

重点看：

```
Features: ... asimd ...
CPU part: 0xd04
```

说明：

```
CPU part 0xd04 -> Cortex-A53
asimd -> AArch64 下的 NEON/SIMD 能力
```

FFTW 编译示例：

```
./configure \
  --host=aarch64-unknown-linux-gnu \
  --enable-single \
  --enable-shared \
  --enable-neon \
  CC=/path/to/aarch64-unknown-linux-gnu-12.4.0-gcc \
  CFLAGS="--sysroot=/path/to/sysroot -O3 -mcpu=cortex-a53" \
  --prefix=/usr
```

笔记要点：

- `--enable-neon` 把 NEON 优化路径编译进 FFTW。
- `-mcpu=cortex-a53` 让编译器针对 Cortex-A53 生成更合适的指令。
- `-O3` 开启较高等级优化。
- NEON 是否真正生效，还取决于目标 CPU 是否支持 `asimd`。