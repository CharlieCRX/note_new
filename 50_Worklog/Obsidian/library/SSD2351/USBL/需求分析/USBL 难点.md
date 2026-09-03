```
1. 水面 DA 如何保证在 start_sample 发第一个点？
```

靠的不是论文，而是硬件定时架构：

```
AD/DA 共用同一个采样时钟 fs
FPGA 内部维护单调递增 sample_counter
软件提前预约 tx_start_sample = current_sample + guard_samples
DAC DMA/播放队列提前装载波形
FPGA 等 sample_counter == tx_start_sample 时打开 DAC 输出
第一个 DAC sample 与 tx_start_sample 绑定
```

如果是 Linux 用户态收到回调后“立即播放”，那就不能保证。因为 Linux 调度、DMA 提交、驱动延迟都会抖动。当前项目文档里要求 `start_sample`，本质就是要求 **硬件定时播放**，不是软件即时播放。

最理想的做法是：

```
1. 用低抖动晶振/PLL 生成统一 200 kHz 采样时钟。
2. 水面 ADC 和 DAC 使用同源时钟。
3. 水下 ADC 和 DAC 使用同源时钟。
4. FPGA 用采样时钟递增 sample_counter。
5. PPS 到来时 latch 当前 sample_counter。
6. 验证相邻 PPS 之间是否等于 200000 samples。
7. DAC 用硬件比较器：sample_counter == start_sample 时输出第一个点。
8. ADC 数据块携带 first_sample，检测峰值换算成 arrival_sample。
```

```
2. AD 如何确保收到 HFM 第一个帧时记录时间？
```

严格说不是“第一个声波刚到就立刻打一个中断时间戳”，而是：

```
AD 每个 sample 都天然带 sample index
五通道数据块带 first_sample
算法在连续样本流里做 HFM matched filter / FISTA / 峰值检测
检测到相关峰位置 peak_index
由 peak_index 反推出 HFM 到达的绝对 sample index
```

也就是说，接收时间戳来自 **采样编号 + 信号检测峰值位置**，不是来自 Linux 函数调用时刻。

当前项目应该追求的闭环是：

```
surface_response_arrival_sample
- query_tx_start_sample
- query_frame_samples
- response_delay_samples
```

这些量全部在采样时间线上表达。这样 RTT 才干净。

所以，论文可以写进技术依据里：

- PONTUS 支撑：USBL interrogation/RTT、固定应答延迟扣除、matched filter TOA、TDOA/DOA。
- Remote Sensing 2023 支撑：USBL travel-time 模型里必须考虑硬件延迟、声速、系统误差。
- Frontiers 2021 支撑：相关法确定 travel-time，TOA 精度直接影响测距。

但验收当前系统时，更关键的是做这些实测：

```
DA 输出回环到 AD，验证 tx_start_sample 与实际接收峰值差是否恒定
多次重复，统计 jitter
水下 response_delay_samples 回环验证
五通道同步性验证
固定线缆/水槽距离标定系统延迟
不同距离下验证 RTT range 线性
```

一句话：**有论文支撑 TWTT/USBL 的测时原理；但 `start_sample` 是否可信，是你们硬件时间线实现和标定报告要证明的事情。**