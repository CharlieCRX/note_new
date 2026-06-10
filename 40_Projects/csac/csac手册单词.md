# CSAC manual

- re-center

  ```
  重新定位
  ```

- prior to

  ```
  等同于 before（在……之前）
  
  语气与语境： 相比于 before，prior to 更加正式、书面，常用于商业、技术、法律或官方语境中（例如：prior to departure 起飞前，prior to shipment 发货前）。
  
  词性解释： prior 本身是形容词（在先的，在前的），但加上 to 之后，整个短语起到了介词的作用，后面直接跟名词（shipment 发货）。
  
  例句：All CSACs have their output frequency re-centered prior to shipment.
  ```

- have something done

  ```
  have + 宾语 (something) + 过去分词 (done) == 使役结构
  
  这个句型通常表示两种情况：
  
  1. 让别人做某事 / 将某事交由别人处理： （主语本身不亲自做）。
  
  	例： - I had my car repaired. (我找人把我的车修好了。)
  		- I had my hair cut yesterday.
  			（我昨天去剪头发了。—— 实际上是理发师帮我剪的。）
  		- You should have your car serviced before the road trip.
  			（在自驾游之前，你应该把车开去打理/保养一下。—— 维修工代劳。）
  		- I got the development environment set up.	
  			(我请别人把我的开发环境布置好了)
  		- 对应本文： 制造商（或系统）将 CSAC 的频率进行了重新校准。
  
  2. 事物经历某种被动的状态：
  
  例： The system had its data wiped. (系统的数据被清除了。)
  ```

- should be expected

  ```
  “是正常的”、“是必然会存在的”、“需要做好心理准备” 或 “应当予以考虑”。
  
  
  -- 它不是“命令”，而是“推论”
  
  
  Therefore, some unknown frequency offset should be expected when the CSAC is first powered on by the user.
  ```

- principal

  ```
  最重要的，首要的
  ```

- Principal RF output

  ```
  主要射频输出
  
  解释： RF 是 Radio Frequency（射频）的缩写，通常指高频交流信号。在原子钟的语境下，这就是它最核心的、用来作为频率基准的信号（例如常见的 10 MHz 或者是 16.384 MHz 方波信号）。
  ```

- TCXO

  ```
  Temperature-Compensated Crystal Oscillator，温补晶振
  
  解释： 普通的石英晶振会随着外界温度的变化而发生频率漂移（热胀冷缩导致走时不准）。TCXO 内部自带了一套温度补偿电路，能顶住环境温度的变化，保持极高的频率稳定性。
  
  你可以把它理解为一块“底子很好、会看温度的普通手表”。
  
  - 普通晶振： 所有的电子设备里都有一个叫“晶振”（石英晶体振荡器）的东西，它通过一秒钟振动几千万次来帮芯片数着时间，就像手表里的齿轮。但石英片会受热胀冷缩影响，夏天热了走得慢，冬天冷了走得快。
  
  - 温补晶振 (TCXO)： 科学家在晶振旁边加了一个温度传感器。当它发现温度变高、晶振变慢时，内部电路就会自动“推一把”；温度低了就“拉一把”。这样一来，它就比普通晶振准了几十倍。
  
  - 它的局限： 它终究是人造的石英片，用个几年就会“变老”（老化），还是会慢慢变慢。
  
  硬核补充： 在 CSAC（原子钟）里，这个 TCXO 实际上是“底层打工人”，它负责输出物理信号，而原子时钟的物理量子部分（如铯原子气室）是“总指挥”，负责实时纠正这个 TCXO 的漂移，两者结合才实现了超高精度。
  ```

- resonance

  ```
  共鸣, 回声, 反响, 谐振, 共振
  
  resonance frequency = 共振频率
  ```

- optical

  ```
  光学上的, 光学的
  ```

- ground state

  ```
  基态
  
  电子能级： E3 、E2、E1
  最低那个：E1 就是 ground state
  
  反义词：excited state --> 激发态
  ```

- order of magnitude

  ```
  数量级
  
  一个数量级：10 倍
  两个数量级：100 倍
  ```

- In addition to 

  ```
  语法点： In addition to 是一个固定介词短语，意为“除了……之外（还包括）”，后面必须跟名词。
  
  In addition to the TCXO and the physics package
  == 除了我们前面已经聊过的温补晶振（TCXO）和物理组件之外
  ```

- Microwave synthesizer

  ```
  微波合成器 = 相当于翻译官
  晶振输出的是普通的频率，需要通过这个合成器，精准地把它放大调制到铯原子能听懂的 4.6GHz 或 9.2GHz 微波频率。
  ```

- conclusion

  ```
  结论、结尾
  
  At the conclusion of the acquisition sequence
  -> 采集序列的结尾
  ```

- tuning voltage

  ```
  调节的电压值
  
  memorizes its last-known-good tuning voltage
  -> CSAC 记忆最后一次好的调节电压
  ```

- calibration

  ```
  测量、校准
  ```

  