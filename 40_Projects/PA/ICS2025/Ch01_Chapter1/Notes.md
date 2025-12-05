# PA1：最简单的计算机

## 计算机采用指令序列执行

将C语言编译为可执行文件后，其本质是将其化为了一条条的指令。从此以后, 计算机就只需要做一件事情:

```c
while (1) {
  从PC指示的存储器位置取出指令;
  执行指令;
  更新PC;
}
```

那么为什么CPU不直接实现C语言的功能，而是做“指令”这样的事情呢？

这是一个非常好的问题，它触及了计算机科学中最核心的概念之一：**抽象（Abstraction）**。

- CPU之所以只做“指令”这样的事情，是因为它追求的是**最基础、最快速、最通用、最可控**的计算单元。
- 而C语言这样的高级语言，追求的是**开发效率、代码可读性、跨平台性**。
- **编译器**就是连接这两者之间的桥梁。它将程序员用C语言表达的复杂意图，翻译成CPU能够理解和执行的一系列简单指令。

并且如果CPU直接实现C语言的功能，就会被语言粒度给拖累：实现指针、内存管理、变量等。导致电路设计复杂，后期CPU升级较为困难。并且每次C语言标准更新或者引入新的编程范式，CPU也需要跟着更新。

而独立于C语言的指令，可以独立于C语言的发展而演进。

## 最简单的计算机

最简单的真实计算机需要满足哪些条件:

- 结构上, TRM有存储器, 有PC, 有寄存器, 有加法器
- 工作方式上, TRM不断地重复以下过程: 从PC指示的存储器位置取出指令, 执行指令, 然后更新PC

NEMU中通过C代码实现了这个基本的计算机，称之为“客户计算机”。

- 客户计算机：NEMU中模拟的计算机
- 客户程序：NEMU中运行的程序

## kconfig生成的宏与条件编译

宏展开`nemu/include/macro.h`中的`IFDEF`。我们以下例分析：

```C
#define DEBUG 1
IFDEF(DEBUG， printf("DEBUG\n");)
```

在理解宏定义之前，需要补充一个知识：宏实参替换发生在形参代入之前

### 宏实参替换发生在形参代入之前

举个小例子

```C
#define DEBUG 1
#define concat_temp(x, y) x ## y

concat_temp(1, DEBUG)
```

编译器在处理 `concat_temp(1, DEBUG)` 的时候分几步：

1. 先找到宏调用

   预处理器看到 `concat_temp(1, DEBUG)`，知道它是个宏调用。

2. 先展开实参

   规则：**实参在传给形参之前，要先各自展开（除非被 `#` 或 `##` 操作符保护）**。

   - 第一个实参是 `1`，它不是宏，展开后还是 `1`。
   - 第二个实参是 `DEBUG`，它是宏，展开成 `1`。

   所以得到：

   ```C
   concat_temp(1, 1)
   ```

   这一步就是“实参替换发生在形参代入之前”。

3. 再把形参代入宏体

   宏体为

   ```C
   x ## y
   ```

   得到：

   ```C
   1 ## 1
   ```

   最终结果：

   ```C
   11
   ```

### 展开`IFDEF(DEBUG， printf("DEBUG\n");)`

有了上面`实参先展开后再传入形参`的知识背景后，我们继续分析这个语句最终被展开为什么了。

```C
#define DEBUG 1
IFDEF(DEBUG， printf("DEBUG\n");)

// 实参 DEBUG 先展开为 1 再传入
= MUXDEF(1, __KEEP, __IGNORE)(printf("DEBUG\n");)
= MUX_MACRO_PROPERTY(__P_DEF_, 1, __KEEP, __IGNORE)(printf("DEBUG\n");)
= MUX_WITH_COMMA(concat(__P_DEF_, 1), __KEEP, __IGNORE)(printf("DEBUG\n");)
= MUX_WITH_COMMA(__P_DEF_1, __KEEP, __IGNORE)(printf("DEBUG\n");)

// 实参 __P_DEF_1 先展开为 'X,' 再传入。这里注意多了个逗号
= MUX_WITH_COMMA(X,, __KEEP, __IGNORE)(printf("DEBUG\n");)
= CHOOSE2nd(X, __KEEP, __IGNORE)(printf("DEBUG\n");)
= __KEEP(printf("DEBUG\n");)

= printf("DEBUG\n");
```

而仔细分析可以得知：

- 当宏被声明并定义为值 1 或者 0 的时候，才能在`IFDEF`宏中执行后面的语句
- 如果宏没有被声明，或者定义的值不为 1 或 0，那么`IFDEF`宏就不会执行后面的语句 

## 参数从哪里来

在`monitor.c`中的函数`parse_args(argc, argv)`，是从哪里输入的参数呢？看下编译过程

```bash
crx@ubuntu:nemu$ make run
/home/crx/study/2025/ics2024/nemu/build/riscv32-nemu-interpreter --log=/home/crx/study/2025/ics2024/nemu/build/nemu-log.txt  
[src/utils/log.c:30 init_log] Log is written to /home/crx/study/2025/ics2024/nemu/build/nemu-log.txt
```

这就是nemu运行的真正过程，入参为`--log=...`。

所以`make run`最终是调用了`riscv32-nemu-interpreter`，并且传入了参数：

- `--log=/home/crx/study/2025/ics2024/nemu/build/nemu-log.txt `

```bash
/home/crx/study/2025/ics2024/nemu/build/riscv32-nemu-interpreter 
--log=/home/crx/study/2025/ics2024/nemu/build/nemu-log.txt  
```

我们看下，项目是如何给`riscv32-nemu-interpreter`传入参数。在`Makefile`中，我们看到调用了`native.mk`，进入此文件可以看到（通过搜索关键字`nemu-log.txt`）：

```makefile
# Some convenient rules

override ARGS ?= --log=$(BUILD_DIR)/nemu-log.txt
override ARGS += $(ARGS_DIFF)

# Command to execute NEMU
IMG ?=
NEMU_EXEC := $(BINARY) $(ARGS) $(IMG)

run-env: $(BINARY) $(DIFF_REF_SO)

run: run-env
	$(NEMU_EXEC)
```

可以看出如果 `ARGS_DIFF` 没有定义或为空，那 `ARGS` 就还是只有 `--log=...`。而最终的编译结果就是并没有加入`ARGS_DIFF`的相关参数，证明并没有定义这个宏。

所以我们可以通过修改`native.mk`来修改`nemu`的入参。

## init_mem()

NEMU 调用`init_monitor()`进行初始化工作的时候，调用了函数`init_mem()`做了一些内存方面的初始化工作。下面就结合代码理解下（`src/memory/paddr.c`中）：

```C
// 定义一个 128 MB 的全局静态内存数组 pmem，
// 并且要求 起始地址必须是 4096 字节对齐，内容初始化为 0。
#define PG_ALIGN __attribute((aligned(4096)))
#define CONFIG_MSIZE 0x8000000
static uint8_t pmem[CONFIG_MSIZE] PG_ALIGN = {};


void init_mem() {
#if   defined(CONFIG_PMEM_MALLOC)
  pmem = malloc(CONFIG_MSIZE);
  assert(pmem);
#endif
  IFDEF(CONFIG_MEM_RANDOM, memset(pmem, rand(), CONFIG_MSIZE));
  Log("physical memory area [" FMT_PADDR ", " FMT_PADDR "]", PMEM_LEFT, PMEM_RIGHT);
}
```

展开其宏定义，代码展示为：

```C
static uint8_t pmem[0x8000000] __attribute((aligned(4096))) = {};
void init_mem() {
  memset(pmem, rand(), 0x8000000);
  Log("physical memory area [" 0x%08x ", " 0x%08x "]", 0x80000000, 0x87FFFFFF));
}
```

这个函数的作用就是：

- 初始化了一段虚拟的物理内存，范围是 **0x80000000 ~ 0x87FFFFFF**，大小为 128 MB

## 客户程序放在哪了

`init_isa()`函数(在`nemu/src/isa/riscv32/init.c`中定义)进行两个操作：

```C
void init_isa() {
  /* Load built-in image. */
  memcpy(guest_to_host(RESET_VECTOR), img, sizeof(img));

  /* Initialize this virtual computer system. */
  restart();
}
```

- 第一项是将一个内置的客户程序读入到内存中
- 第二项任务是初始化寄存器

那我们就聚焦于第一步，看看客户程序放在哪了。

首先客户程序是一个riscv的程序：

```C
// this is not consistent with uint8_t
// but it is ok since we do not access the array directly
static const uint32_t img [] = {
  0x00000297,  // auipc t0,0
  0x00028823,  // sb  zero,16(t0)
  0x0102c503,  // lbu a0,16(t0)
  0x00100073,  // ebreak (used as nemu_trap)
  0xdeadbeef,  // some data
};
```

可以看出`img`是一个`uint32_t`的数组。这里解释下为什么一个看起来有点“奇怪”的声明方式是可接受的：

> 由于 **RISC-V 32 位架构指令长度为 32 位**的特性，使得 `uint32_t` 成为存储这些指令的理想数据类型。
>
> 注释只是提醒你，尽管在内存层面这些数据最终还是以字节形式存在，但在 C 代码层面，只要你不进行字节级的直接访问，将它们视为 32 位整数是完全没有问题的。

客户程序被分配到了`guest_to_host(RESET_VECTOR)`的位置。如果想知道存放在哪里，就需要理解函数`guest_to_host(paddr)`（在`src/memory/paddr.c`）。

```C
#define CONFIG_MBASE 0x80000000
#define CONFIG_MSIZE 0x8000000
#define PG_ALIGN __attribute((aligned(4096)))

#if defined(CONFIG_PMEM_MALLOC)
static uint8_t *pmem = NULL;
#else
static uint8_t pmem[CONFIG_MSIZE] PG_ALIGN = {};
#endif

typedef MUXDEF(PMEM64, uint64_t, uint32_t) paddr_t;

uint8_t* guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
```

这个函数的目标是将 **客户机（guest）物理地址 `paddr`**，转换成在 **宿主机（host）上的内存地址**（即实际内存访问地址）。它本质上构建了一个从“客户机物理地址空间”到“宿主机进程虚拟地址空间”的 **线性映射**。下面是详细分析：

- **`CONFIG_MBASE`**: 这是一个宏，定义了客户机（guest）物理内存的**起始基地址**。意味着对于客户机程序来说，它的内存地址从 `0x80000000` 开始。

- **`PG_ALIGN`**: 这是一个宏，用于内存对齐。

  - **`__attribute((aligned(4096)))`**，这是 GCC (GNU Compiler Collection) 编译器的一个扩展属性。
  - **`aligned(4096)`**，表示被这个属性修饰的变量（或内存块）的起始地址必须是 `4096` 字节（即 4KB）的整数倍。

- `pmem`为客户程序的物理内存。

  - `static uint8_t pmem[CONFIG_MSIZE]`: `pmem` 被声明为一个静态的 `uint8_t` 数组。其大小为`CONFIG_MSIZE`，即为`0x8000000`个字节。
  - `PG_ALIGN`: 这个宏在这里生效，确保 `pmem` 数组的起始地址是 4096 字节对齐的。

  这种方式会在程序编译链接时就为 `pmem` 分配固定`128MB`大小的内存空间，通常在程序的 BSS 段或数据段。适用于内存大小固定且在编译时已知的场景。

- 地址转换函数`guest_to_host(paddr_t)`

  - **`paddr_t` **是用来表示**“客户机的物理地址”**的专用类型，代表客户程序眼中的真实内存位置。
  - `paddr_t` 表示客户程序发出的物理地址（如取指令、读写数据）
  - `guest_to_host` 把它映射到宿主机中真正的内存指针（模拟内存）

- `paddr - CONFIG_MBASE`

  - 因为客户机物理内存是从 `0x80000000` 开始的，所以减去 `CONFIG_MBASE`，相当于计算客户机地址在 pmem 这块内存中的偏移。

    例如：

    ```C
    paddr = 0x80001000
    CONFIG_MBASE = 0x80000000
    offset = 0x1000
    ```

- ### `pmem + offset`

  - 最后将这个偏移加到 `pmem` 上，得到的是：宿主机进程中实际要访问的虚拟地址。
  - 表示 “客户机地址 `0x80001000` 映射到宿主机中 `pmem + 0x1000`”

现在已经知道的是`RESET_VECTOR `值为`0x80000000`：

```C
guest_to_host(0x80000000) == pmem + (0x80000000 - 0x80000000) == pmem
```

也就是说，客户程序从 `0x80000000` 开始，就被加载到 `pmem[0]` 开始的位置（主机位置）。这也说明：

- 客户程序实际在宿主机的 `pmem[0]` 开始存放。
- `guest_to_host()` 提供了一个从 **客户机视角地址空间** 到 **宿主机地址空间的访问手段**。

### `guest_to_host(paddr)`

- **功能**：将 guest 物理地址（以 `CONFIG_MBASE` 为起点）映射到 host 中 `pmem` 的偏移地址。
- **意义**：在模拟器或软硬件协作系统中，让 host 系统能够读写客户机的内存。
- **关键前提**：客户机所有地址都必须在 `[CONFIG_MBASE, CONFIG_MBASE + CONFIG_MSIZE)` 之间，否则越界访问。

### 检验指令加载流程

我们不妨做个测试：

> ✅验证客户程序被正确地加载到了 guest 的 `0x80000000` 地址（即 host 的 `pmem[0]`），与 `guest_to_host(paddr)` 的理论一致。

```bash
(gdb) b monitor.c:118                                                                                                                     Breakpoint 1 at 0x4756: file src/monitor/monitor.c, line 118.                                                                             (gdb) run                                                                                                                                 Starting program: /home/crx/study/2025/ics2024/nemu/build/riscv32-nemu-interpreter --log=/home/crx/study/2025/ics2024/nemu/build/nemu-log.txt                                                                    
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
[src/utils/log.c:30 init_log] Log is written to /home/crx/study/2025/ics2024/nemu/build/nemu-log.txt
[src/memory/paddr.c:50 init_mem] physical memory area [0x80000000, 0x87ffffff]

Breakpoint 1, init_monitor (argc=<optimized out>, argv=<optimized out>) at src/monitor/monitor.c:118
118       init_isa();
(gdb) p guest_to_host(RESET_VECTOR)
// pmem 是模拟的物理内存，位于 host 的虚拟地址空间 0x55555555f000。
$1 = (uint8_t *) 0x55555555f000 <pmem> '\340' <repeats 199 times>, <incomplete sequence \340>...
(gdb) x pmem
0x55555555f000 <pmem>:  0xe0e0e0e0
(gdb) n	// 此时已经执行完毕 memcpy(...);
42        restart();
(gdb) set $addr = guest_to_host (RESET_VECTOR)
(gdb) x/5xw $addr
0x55555555f000 <pmem>:  0x00000297      0x00028823      0x0102c503      0x00100073
0x55555555f010 <pmem+16>:       0xdeadbeef
(gdb) x/5xw pmem
0x55555555f000 <pmem>:  0x00000297      0x00028823      0x0102c503      0x00100073
0x55555555f010 <pmem+16>:       0xdeadbeef
```

## 基础设施

### 打印寄存器

这里需要用户在输入`info r`后打印所有寄存器的值。首先就是需要找到存放寄存器值的位置在哪里。

根据之前 RTFSC 章节，可以得知：在 NEMU 的框架代码中，CPU 的所有寄存器（如通用寄存器、程序计数器 **PC** 等）的数据会被组织在一个**数据结构**中，这个结构体代表了 CPU 的当前状态。

寄存器结构体在`src/isa/riscv32/include/isa-def.h`中定义：

```C
typedef struct {
  word_t gpr[MUXDEF(CONFIG_RVE, 16, 32)];
  vaddr_t pc;
} MUXDEF(CONFIG_RV64, riscv64_CPU_state, riscv32_CPU_state);
```

并且根据宏定义以及配置文件生成的`autoconf.c`，以及`.config`中的描述：

```ini
# CONFIG_RV64 is not set
# CONFIG_RVE is not set
```

可以得知 riscv32 的寄存器结构体为：

```C
typedef struct {
  word_t gpr[32]; // RISC-V 32-bit (RV32I) 标准有 32 个通用寄存器 (x0-x31)
  vaddr_t pc;
} riscv32_CPU_state;
```

并且项目在`nemu/src/cpu/cpu-exec.c`中定义一个全局变量`cpu`:

```C
CPU_state cpu = {};
```

可以得知，寄存器的值，就在变量`cpu`中。

既然找到了寄存器的存放位置，下面就是打印寄存器的值了。

统一的打印寄存器值的函数在对应`$ISA`中，当前架构的函数定义在`src/isa/riscv32/reg.c`下：

```C
void isa_reg_display() {
}
```

打印要求就是将寄存器的所有数据打印：

- 打印 32 个通用寄存器
- 打印 PC

打印形式类似于 gdb 输出：

```bash
(gdb) info r
rax            0x5555555565a9      93824992241065
rbx            0x7fffffffe158      140737488347480
...
```

格式类似于：

```bash
[寄存器名称]       [保存的数据以十六进制展示]     [保存的数据以十进制展示]
```

示例实现：

```C
void isa_reg_display() {
  // 输出所有通用寄存器的值
  for (int i = 0; i < 32; i++) {
    printf("%-3s: 0x%-8x    %-d\n", regs[i], cpu.gpr[i], cpu.gpr[i]);
  }

  // 输出程序计数器的值
  printf("pc : 0x%08x    %-10d\n", cpu.pc, cpu.pc);
}
```

