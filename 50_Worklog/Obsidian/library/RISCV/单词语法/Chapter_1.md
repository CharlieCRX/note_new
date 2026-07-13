# Riscv词汇

- 16-bit instructions

  ```
  指的是一条机器指令的总长度只有 16 个二进制位
  ```

  

- 3-address format

  ```
  指的是一条指令中包含 3 个操作数（操作地址）。在典型的 RISC 架构中，一个运算指令通常需要指定：
  
  	目标寄存器（Destination, 简称 rd）：存放结果的地方。
  
  	源寄存器 1（Source 1, 简称 rs1）：第一个参与运算的数据。
  
  	源寄存器 2（Source 2, 简称 rs2）：第二个参与运算的数据。
  
  例如经典的汇编指令：ADD rd, rs1, rs2 （意思是：rd = rs1 + rs2）。这里面就包含了 3 个“地址”（即 3 个寄存器编号）。
  ```
  
- align

  ```
  （使）排成一直线；使平行
  
  The base ISA has IALIGN=32, meaning that instructions must be aligned on a four-byte boundary in memory.
  “对齐在 4 字节边界上”意味着，任何指令在内存中的存放地址，必须是 4 的整数倍。
  
  合法地址： 0x0000, 0x0004, 0x0008, 0x000C ...
  
  非法地址： 0x0001, 0x0002, 0x0003, 0x0005 ...
  ```

- conditional

  ```
  条件的
  a conditional branch that is not take
  一个没有被执行的条件分支指令
  ```

- reserved

  ```
   保留的, 预备的, 预定的
   
   当（CPU）解码到一条保留指令时，其行为是未指定的（UNSPECIFIED）
  ```

- immediate 

  ```
  立即数 立即的
  反义词：later never
  
  in the immediate value being produced -- 正在被生成的那个立即数值
  being produced 是现在分词短语，
  修饰 the immediate value，即：
  
  the immediate value being produced
  =
  the immediate value that is being produced
  
  省略了 that is
  
  Each immediate subfield is labeled with the bit position (imm[x]) in the immediate value being produced, rather than the bit position within the instruction’s immediate field as is usually done.
  ```

- upon

  ```
  upon = on
  
  在技术文档里经常表示：
  
  当……
  一旦……
  在……时
  
  Upon receiving the packet --> 收到数据包时
  
  The behavior upon decoding a reserved instruction is UNSPECIFIED == The behavior is UNSPECIFIED.
  The behavior
      upon decoding a reserved instruction
  is
      UNSPECIFIED
  
  upon decoding a reserved instruction 是一个介词短语，修饰： The behavior
  
  例句：
  - The behavior upon reset is undefined.
  - The value upon overflow is unspecified.
  ```

- UNSPECIFIED

  ```
  规范故意不规定
  ```

- leftmost

  ```
  最左边的
  
  Except for the 5-bit immediates used in CSR instructions (Chapter 6), immediates are always sign-extended, and are generally packed towards the leftmost available bits in the instruction and have been allocated to reduce hardware complexity 
  
  ---> 拆解
  
    immediate 
  1. are sign-extended
  2. are packed towards leftmost bits
  3. have been allocaterd
  	to reduce hardware complexity
  
  have been allocated = 现在完成时 + 被动语态 == 这些立即数字段已经被(?)安排好了 ==> 
  - 隐含：
  - ISA designers
  ```

- branch offset

  ```
  分支跳转偏移量
  
  - branch = beq x1, x2, label
  - offset = PC + offset
  
  ====> 分支跳转偏移量
  ```

- in multiples of 2

  ```
  2 的倍数
  
  multiple = 倍数
  ```
- separate

  ```
  划分
  
  An ISA separated into a small base integer ISA, usable by itself as a base for customized accelerators or for educational purposes, and optional standard extensions, to support general-purpose software development.
  
  主干：An ISA separated into A and B
  
  usable by itself：
  	过去分词作后置定语。完整应该写成：which is usable by itself
  	它本身就可以单独使用。
  ```

- commentary

  ```
  注释，评论，现场解说，实况报道
  ```

  