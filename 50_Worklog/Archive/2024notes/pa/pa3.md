# PA3 - 穿越时空的旅程: 批处理系统

## PA3.1

### 概览

**:thought_balloon: 实现目标**

通过软硬件协作，实现程序的执行流切换

下面本节具体例子来帮助理解：通过`am-tests`中的`yield test`测试触发一次自陷操作![yield test逻辑](C:\Users\crx\Desktop\pa3\yield.png)

程序内部的执行流切换：函数A和函数B可以简单地通过`call/jal`指令实现；

程序之间的执行流切换：因为安全问题，程序Prog1和程序Prog2的执行流切换只能交给操作系统和硬件来共同实现。

为了满足程序之间的执行流切换，需要硬件和操作系统（或AM）共同协作。

**实现的功能概览**

- 实现“硬件响应机制”
- 实现CTE的异常处理功能

**:warning: 注意**

本节中的描述中，有几个概念需要重重点关注下下。

1. 操作系统：其实是一个能够协调硬件进行程序的执行流切换后台软件罢了。在本节“穿越时空的旅程”中，硬件就是NEMU本身，而与硬件直接交互的“操作系统”的这个后台软件，其实就是库函数AM（CTE）。
2. 异常：用户程序调用硬件提供的执行流切换入口时候，会触发触发硬件的异常处理机制。这里的“异常”并不是通常意义上的错误，而是指执行流切换这样的特殊情况。

### 硬件的异常响应机制

硬件为程序提供了一种可以限制入口的执行流切换方式，这种方式就是自陷指令。硬件接收到程序执行的自陷指令之后, 就会在**硬件的操作**下，陷入到操作系统预先设置好的跳转目标. 这个跳转目标也称为异常入口地址.

而这里提到的**硬件的操作**，叫做“异常响应机制”。“异常响应机制”的步骤是：

1. 保存程序状态
2. 跳转异常入口地址

硬件部分要做的，就是让NEMU硬件实现自陷指令和异常响应机制：

![image-20241108113112390](E:\backup\software\typora_image\image-20241108113112390.png)

- 提供自陷指令`ecall`
- 异常响应机制：`isa_raise_intr()`

下面我们就分析下，在NEMU的硬件(目录`nemu/`)下，怎么用代码实现我们想要的需求。

首先需要对应的硬件来支持。我们用控制状态寄存器（CSR）这类特殊的系统寄存器来保存程序状态；而异常入口地址的保存工作，则交给了系统寄存器mtvec。

在PA中，用到的3个CSR包括：

- mepc寄存器 - 存放触发异常的PC
- mstatus寄存器 - 存放处理器的状态
- mcause寄存器 - 存放触发异常的原因

相应地，riscv32提供`ecall`指令作为自陷指令。程序调用自陷指令后，触发异常后硬件的响应过程`isa_raise_intr()`如下:

![image-20241108163216901](E:\backup\software\typora_image\image-20241108163216901.png)

1. 将当前PC值保存到mepc寄存器
2. 在mcause寄存器中设置异常号
3. 从mtvec寄存器中取出异常入口地址
4. 跳转到异常入口地址

综上，要想让NEMU硬件实现自陷指令和异常响应机制，硬件需要实现的代码为：

- 在CPU寄存器中，添加相应的系统寄存器来支持异常响应机制（在`nemu/src/isa/riscv32/include/isa-def.h`定义）

  ```c
  // 用于控制和监控 CPU 状态的特殊寄存器
  typedef struct control_and_status_registers {
  	word_t mtvec;  // 异常入口地址
  	word_t mepc;   // 触发异常的PC
  	word_t mstatus;// 处理器的状态
  	word_t mcause; // 触发异常的原因
  }CSRs;
  
  // CSR 编号
  typedef enum {
  	CSR_MSTATUS = 0x300, // mstatus
  	CSR_MTVEC = 0x305,   // mtvec
  	CSR_MEPC = 0x341,		 // mepc
  	CSR_MCAUSE = 0x342,  // mcause
  }csr_id;
  
  typedef struct {
    word_t gpr[MUXDEF(CONFIG_RVE, 16, 32)];
    vaddr_t pc;
  	CSRs csrs;
  } MUXDEF(CONFIG_RV64, riscv64_CPU_state, riscv32_CPU_state);
  ```

  并且在寄存器功能（在`nemu\src\isa\riscv32\local-include\reg.h`中定义）实现了读写控制寄存器的功能.

  ```c
  word_t get_csr_val_by_id(int csr_id);
  void set_csr_val_by_id(int csr_id, word_t val);
  
  #define read_csrs(idx) (get_csr_val_by_id(idx)) 
  #define write_csrs(idx, val) (set_csr_val_by_id(idx, val))
  ```

- 下面实现异常响应机制`isa_raise_intr()`（在`nemu\src\isa\riscv32\system\intr.c`中定义）

  ```C
  word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  	// 保存程序状态
  	cpu.csrs.mepc = epc;
  	cpu.csrs.mcause = NO;
  
  	// 跳转异常处理入口地址
  	return cpu.csrs.mtvec;
  }
  ```

- 在riscv32指令集（在`nemu\src\isa\riscv32\inst.c`）中，加入自陷指令`ecall`、`csrrw`的译码处理

  ```c
  INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall  , N, s->dnpc = isa_raise_intr(16, s->pc));
  INSTPAT("??????? ????? ????? 001 ????? 11100 11", csrrw  , I, R(rd) = read_csrs(imm); write_csrs(imm, src1));
  ```
  

### 软件的异常处理功能

异常入口地址是操作系统指定给硬件的。要想实现异常处理功能，抽象操作系统有两个主要职责：

1. 在硬件中设置异常处理程序的入口地址
2. 通过异常处理程序来处理用户程序的异常请求

而在本节涉及到的操作系统概念，同等与AM中专门负责异常处理的模块CTE。下面我们分析下CTE的主要功能有哪些。

- 首先是初始化CTE。在初始化的时候，注册一个事件处理回调函数。后续异常处理程序会调用事件处理回调函数，灵活地处理相应事件。另外初始化CTE的同时，也会设置异常处理程序的入口地址。

- 其次因为用户程序运行在AM之上，要想架构无关地调用硬件提供的自陷指令，需要AM提供包装后的自陷函数。

- 另外还要提供异常处理程序来处理异常。![image-20241108125044870](E:\backup\software\typora_image\image-20241108125044870.png)

除了CTE的基本功能，还有两个类型的辅助变量来协助处理异常事件。

- 为了区分不同的执行流切换原因，CTE将切换原因定义为一个“事件”结构体`Event`。

- 为了保存和使用程序状态信息，CTE将异常状态下的程序信息保存为一个上下文结构体`Context`。

#### 初始化CTE

![image-20241108125900771](E:\backup\software\typora_image\image-20241108125900771.png)

```C
bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // register event handler
  user_handler = handler;

  return true;
}
```

需要在NEMU硬件中实现指令`csrw`的译码过程。

#### 自陷函数

```c
void yield() {
  asm volatile("li a7, -1; ecall");
}
```

- 触发自陷事件：`li a7, -1`
- 执行自陷指令：`ecall`。

在NEMU硬件中，我们已经实现了自陷指令`ecall`的译码处理。

#### 异常处理

`__am_asm_trap`（在`abstract-machine\am\src\riscv\nemu\trap.S`中定义）

![image-20241108133011175](E:\backup\software\typora_image\image-20241108133011175.png)

**保存上下文**

上下文（结构体`Context`）包括：通用寄存器信息、系统寄存器信息和地址空间。在riscv32中，将地址空间信息与0号寄存器（`gpr[0]`）共用存储空间。结合代码和栈空间理解保存上下文的作用：

![image-20241110005958950](E:\backup\software\typora_image\image-20241110005958950.png)

**异常处理函数**

在这里我们要解决的两个问题：

- 改写`Context`结构成员顺序
- 让函数`__am_irq_handle()`从上下文中正确识别处自陷事件

首先理解栈的成员访问和结构体成员访问的关系，改写`Context`结构成员顺序。

上下文保存完毕后，此时首先将栈指针`sp`赋值给寄存器`a0`。在riscv32中，寄存器`a0`用来传递函数参数。随后调用异常处理函数`__am_irq_handle`：

```c
Context* __am_irq_handle(Context *c);
```

函数入参为上下文结构指针`c`, `c`指向的上下文结构，就是来源于栈顶指针`sp`。在栈帧中，结构体成员的顺序遵循它们在结构体中定义的顺序。栈帧中的成员在内存中的布局如下：

- 靠近栈顶指针的成员：通常为结构体的第一个成员
- 远离栈顶指针的成员：在结构体定义中出现较晚的成员，位于内存的高地址部分。

所以根据栈保存成员的顺序，可以很轻松地改写上下文结构体`Context`的成员顺序。

```c
struct Context {
  uintptr_t gpr[NR_REGS], mcause, mstatus, mepc;
  void *pdir;
};
```

这里PA手册中，对于地址空间的保存位置，让我有些迷惑，等日后再来重新理解：

> 地址空间：...mips32和riscv32则是将地址空间信息与0号寄存器共用存储空间, 反正0号寄存器的值总是0, 也不需要保存和恢复. 
>
> 这里让我觉得是保存0号寄存器的栈空间，现在是保存上下文的地址空间信息，所以我一开始将`pdir`放在了开头。然而这样是错误的。

下一步分析函数`__am_irq_handle()`的接口

- 将程序的执行流切换原因打包为事件
- 调用注册好的事件处理函数，处理此事件

STFM后，可以得知：在 RISC-V 中，`mcause` 寄存器用于指示异常（exception）或中断（interrupt）的原因。当程序执行流切换到异常处理程序时，`mcause` 寄存器记录了异常的原因。如果程序的执行流切换原因为自陷（`ecall`），我们可以通过检查 `mcause` 寄存器的值来确认这一点。

```c
Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
	  case 16:				  ev.event = EVENT_YIELD;	break;
      default:				  ev.event = EVENT_ERROR;   break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}
```

**恢复上下文**

在异常处理程序`__am_asm_trap`执行完异常处理函数`__am_irq_trap()`后，会再次回到异常处理程序中。此时异常处理程序要做的就是根据上下文信息，恢复程序的状态。

这一步骤是从栈中更新寄存器的值，执行流程为

- 更新mstatus

- 更新epc

  因为此时riscv32寄存器`epc`保存的是调用`ecall`指令时候的地址，所以恢复`epc`的值后，需要对保存的PC加上4, 使得将来返回到自陷指令的下一条指令

- 更新通用寄存器

```assembly
  #...
  jal __am_irq_handle

  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1

  #将 mepc + 4，返回到 `ecall` 指令后的下一条指令
  addi t2, t2, 4
  csrw mepc, t2

  MAP(REGS, POP)
  #...
```

这样程序触发异常之前的状态就被恢复了，并且更新了`epc`为异常处理完成后的下一条指令地址。

**异常返回指令**

riscv32通过`mret`指令从异常处理过程中返回, 它将根据mepc寄存器恢复PC.

![image-20241108155136081](E:\backup\software\typora_image\image-20241108155136081.png)

### 总结

至此，除了etrace，PA3.1终结。

问题就是卡在了对于上下文的成员顺序的理解错误。

还有回顾时间太久。

