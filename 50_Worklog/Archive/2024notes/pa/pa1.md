# PA1 - 开天辟地的篇章: 最简单的计算机

## 在开始愉快的PA之旅之前

:label: 对于即将面临的材料描述不充分的问题，PA1这么介绍：

> 做PA的终极目标是通过构建一个简单完整的计算机系统, 来深入理解程序如何在计算机上运行. 和那些"用递归实现汉诺塔"的程序设计作业不同, 计算机系统比汉诺塔要复杂得多. 这意味着, 通过程序设计作业的训练方式是不足以完成PA的, 只有去尝试理解并掌握计算机系统的每一处细节, 才能一步步完成PA.
>
> 所以, 不要再用程序设计作业的风格来抱怨PA讲义写得不清楚, 之所以讲义的描述点到即止, 是为了强迫大家去理清计算机系统的每一处细节, 去推敲每一个模块之间的关系, 也是为了让大家积累对系统足够的了解来面对未知的bug.
>
> 这对你来说也许是一种前所未有的训练方式, 所以你也需要拿出全新的态度来接受全新的挑战.

告诉我们，材料可能只告诉了你目的地在哪，至于地图在哪，路线怎么规划，就需要自己探索了:earth_africa:。

:flags:这里还讲到了`通过STFW和RTFM独立解决问题"的最初原的信念, 这种信念可以帮助大家驱散对未知的恐惧`。

但是说句实在的，我通过`man`命令调出来的黑乎乎的命令指示，还是相当头晕目眩的😨，有点恐惧于`man`命令的体量，然后经常付诸于google的中文引擎去搜索对应的命令常用用法。

或许我应该弄明白，如何看懂`man operation`以及何时应该去看这个吓人的手册，希望经过我的学习，能让我的`STFM`过程更加高效有趣。😊

### 更快的编译速度

:taco: 小练习：

> 为了使用`ccache`, 你还需要进行一些配置的工作. 首先运行如下命令来查看一个命令的所在路径:
>
> ```bash
> which gcc 
> ```
>
> 作为一个RTFM的练习, 接下来你需要阅读`man ccache`中的内容, 并根据手册的说明, 在`.bashrc`文件中对某个环境变量进行正确的设置. 如果你的设置正确且生效, 重新运行`which gcc`, 你将会看到输出变成了`/usr/lib/ccache/gcc`. 如果你不了解环境变量和`.bashrc`, STFW.

在目录`usr/lib/ccache`下，包含了各种编译器链接到`ccache`的符号链接：

```bash
crx@ubuntu:ccache$ ll
total 8
drwxr-xr-x   2 root root 4096 Aug  7 16:21 ./
drwxr-xr-x 112 root root 4096 Aug  7 16:21 ../
lrwxrwxrwx   1 root root   16 Aug  7 16:21 c++ -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 c89-gcc -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 c99-gcc -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 cc -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 g++ -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 g++-11 -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 gcc -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 gcc-11 -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 x86_64-linux-gnu-g++ -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 x86_64-linux-gnu-g++-11 -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 x86_64-linux-gnu-gcc -> ../../bin/ccache*
lrwxrwxrwx   1 root root   16 Aug  7 16:21 x86_64-linux-gnu-gcc-11 -> ../../bin/ccache*
```

所以在调用此目录下的gcc命令时，实际上调用的是ccache，这样就实现了缓存编译的目的。

但是这就需要将此目录设置为`PATH`变量中，较原版`gcc`更靠前的位置，而这就牵扯出了环境变量的概念。

简单来说，环境变量相当于shell的配置，让它知道在哪里能找到某些资源。

而环境变量中`PATH`变量的作用，是让系统快速启动一个应用程序。他告诉操作系统去哪些目录中寻找可执行程序。`PATH`变量的变量值是多个文件夹的路径。当我们在命令行中输入一个命令的时候，操作系统会按照`PATH`变量中列出的目录顺序，按序查找这个命令的可执行文件。

`PATH`变量包含很多文件夹的路径：

```bash
crx@ubuntu:~$ echo $PATH
/usr/lib/ccache:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin
crx@ubuntu:~$
```

这里在环境变量的配置文件`~/.bashrc`中，加入了

```bash
export PATH=/usr/lib/ccache:$PATH
```

这样就会让`/usr/lib/ccache`目录变量在整个`PATH`变量中，作为最靠前的变量进行检索，这样使用gcc编译文件的时候，就相当于执行ccache了。

### NEMU是什么

NEMU是一个模拟器:desktop_computer: ，模拟出一个计算机的所有硬件。应该是要模拟出一个内存、cpu、缓存等硬件系统。

##  开天辟地的篇章

从题目理解状态机：

> ##### :paintbrush: 从状态机视角理解程序运行
>
> 以上一小节中`1+2+...+100`的指令序列为例, 尝试画出这个程序的状态机.
>
> 这个程序比较简单, 需要更新的状态只包括`PC`和`r1`, `r2`这两个寄存器, 因此我们用一个三元组`(PC, r1, r2)`就可以表示程序的所有状态, 而无需画出内存的具体状态. 初始状态是`(0, x, x)`, 此处的`x`表示未初始化. 程序`PC=0`处的指令是`mov r1, 0`, 执行完之后`PC`会指向下一条指令, 因此下一个状态是`(1, 0, x)`. 如此类推, 我们可以画出执行前3条指令的状态转移过程:
>
> ```tex
> (0, x, x) -> (1, 0, x) -> (2, 0, 0) -> (3, 0, 1)
> ```
>
> 请你尝试继续画出这个状态机, 其中程序中的循环只需要画出前两次循环和最后两次循环即可.

答：

```TEX
(0, x, x) -> (1, 0, x) -> (2, 0, 0) -> (3, 0, 1) -> (4, 1, 1)
-> (2, 1, 1) -> (3, 1, 2) -> (4, 3, 2)
……
-> (2, 1+2+...+98, 98) -> (3, 1+2+...+98, 99) -> (4, 1+2+3+...+99, 99)
-> (2, 1+2+...+99, 99) -> (3, 1+2+...+99, 100) -> (4, 1+2+3+...+99+100, 100)
```

这样将PC、r1和r2的值作为状态机的状态，然后执行程序的流程体现在状态的转移过程，就可以很好地理解其实程序执行无非也是状态的转移：从一个状态（时序逻辑），在组合逻辑（代码流程）的作用下，计算并转移到下一时钟周期的新状态。

##  RTFSC

:bookmark_tabs:在阅读`friendly source code`的时候，遇到了makefile文件:`/ics2023/nemu/scripts/build.mk`

其中涉及到的某些Makefile的语法这里简单介绍下，方便阅读。

**模式替换**

```makefile
OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o) $(CXXSRC:%.cc=$(OBJ_DIR)/%.o)
```

将C和C++源文件的对象文件，合并到`OBJ`变量中。/memory/paddr.c

如果 `SRCS` 是 `src/main.c src/utils.c`，而 `CXXSRC` 是 `src/main.cc src/utils.cc`，且 `OBJ_DIR` 是 `build/obj`，则`OBJS` 将包含 `build/obj/main.o build/obj/utils.o build/obj/main.o build/obj/utils.o`。

**`%`**：表示通配符，用于匹配任何字符，包括空字符。例如，`%.c` 匹配所有以 `.c` 结尾的文件名。

如何理解：

```makefile
# Compilation patterns
$(OBJ_DIR)/%.o: %.c
  @echo + CC $<
  @mkdir -p $(dir $@)
  @$(CC) $(CFLAGS) -c -o $@ $<
  $(call call_fixdep, $(@:.o=.d), $@)
```

不妨先查看`make`过程中都运行了哪些命令，然后反过来理解`$(CFLAGS)`等变量的值。 为此, 我们可以键入`make -nB`, 它会让`make`程序以"只输出命令但不执行"的方式强制构建目标. 运行后, 你可以看到很多形如

```bash
echo + CC src/memory/paddr.c

mkdir -p /home/crx/study/ics2023/nemu/build/obj-riscv32-nemu-interpreter/src/memory/

gcc -O2 -MMD -Wall -Werror 
-I/home/crx/study/ics2023/nemu/include 
-I/home/crx/study/ics2023/nemu/src/engine/interpreter 
-I/home/crx/study/ics2023/nemu/src/isa/riscv32/include 
-O2    
-DITRACE_COND=true 
-D__GUEST_ISA__=riscv32 
-c -o 
/home/crx/study/ics2023/nemu/build/obj-riscv32-nemu-interpreter/src/memory/paddr.o src/memory/paddr.c


/home/crx/study/ics2023/nemu/tools/fixdep/build/fixdep  /home/crx/study/ics2023/nemu/build/obj-riscv32-nemu-interpreter/src/memory/paddr.d  /home/crx/study/ics2023/nemu/build/obj-riscv32-nemu-interpreter/src/memory/paddr.o unused >  /home/crx/study/ics2023/nemu/build/obj-riscv32-nemu-interpreter/src/memory/paddr.d.tmp
mv  /home/crx/study/ics2023/nemu/build/obj-riscv32-nemu-interpreter/src/memory/paddr.d.tmp  /home/crx/study/ics2023/nemu/build/obj-riscv32-nemu-interpreter/src/memory/paddr.d
```

这样可以得出对应变量的值

```TEX
$< -> src/memory/paddr.c
$@ -> /home/crx/study/ics2023/nemu/build/obj-riscv32-nemu-interpreter/src/memory/paddr.o 
$(CC) -> gcc
$(CFLAGS) ->
-MMD -Wall -Werror 
-I/home/crx/study/ics2023/nemu/include 
-I/home/crx/study/ics2023/nemu/src/engine/interpreter 
-I/home/crx/study/ics2023/nemu/src/isa/riscv32/include 
-O2
-DITRACE_COND=true 
-D__GUEST_ISA__=riscv32 
```

而`$(CFLAGS)`前面的定义为

```makefile
CFLAGS  += -fPIC -fvisibility=hidden
CFLAGS  := -O2 -MMD -Wall -Werror $(INCLUDES) $(CFLAGS)
```

所以可以推测

```tex
$(INCLUDES) -> 
-I/home/crx/study/ics2023/nemu/include 
-I/home/crx/study/ics2023/nemu/src/engine/interpreter 
-I/home/crx/study/ics2023/nemu/src/isa/riscv32/include 
```

而`$(INCLUDE)`

```makefile
INC_PATH := $(WORK_DIR)/include $(INC_PATH)
INCLUDES = $(addprefix -I, $(INC_PATH))
```

所以`$(INC_PATH)`为

```tex
$(INC_PATH) ->
/home/crx/study/ics2023/nemu/include 
/home/crx/study/ics2023/nemu/src/engine/interpreter 
/home/crx/study/ics2023/nemu/src/isa/riscv32/include 
```

这里就是简单地从编译结果去理解源变量的意义。

准备第一个客户程序

**init_monitor()函数**

然后把目光转向`nemu/src/monitor/monitor.c`的初始化函数`init_monitor()`——将客户程序读入到客户计算机中。

`init_monitor()`函数的代码如下：

```C
void init_monitor(int argc, char *argv[]) {
  /* Perform some global initialization. */

  /* Parse arguments. */
  parse_args(argc, argv);

  /* Set random seed. */
  init_rand();

  /* Open the log file. */
  init_log(log_file);

  /* Initialize memory. */
  init_mem();

  /* Initialize devices. */
  IFDEF(CONFIG_DEVICE, init_device());

  /* Perform ISA dependent initialization. */
  init_isa();

  /* Load the image to memory. This will overwrite the built-in image. */
  long img_size = load_img();

  /* Initialize differential testing. */
  init_difftest(diff_so_file, img_size, difftest_port);

  /* Initialize the simple debugger. */
  init_sdb();

#ifndef CONFIG_ISA_loongarch32r
  IFDEF(CONFIG_ITRACE, init_disasm(
    MUXDEF(CONFIG_ISA_x86,     "i686",
    MUXDEF(CONFIG_ISA_mips32,  "mipsel",
    MUXDEF(CONFIG_ISA_riscv,
      MUXDEF(CONFIG_RV64,      "riscv64",
                               "riscv32"),
                               "bad"))) "-pc-linux-gnu"
  ));
#endif

  /* Display welcome message. */
  welcome();
}
```

解析函数`parse_args()`：

```C
static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {
    {"batch"    , no_argument      , NULL, 'b'},
    {"log"      , required_argument, NULL, 'l'},
    {"diff"     , required_argument, NULL, 'd'},
    {"port"     , required_argument, NULL, 'p'},
    {"help"     , no_argument      , NULL, 'h'},
    {0          , 0                , NULL,  0 },
  };
  int o;
  while ( (o = getopt_long(argc, argv, "-bhl:d:p:", table, NULL)) != -1) {
    switch (o) {
      case 'b': sdb_set_batch_mode(); break;
      case 'p': sscanf(optarg, "%d", &difftest_port); break;
      case 'l': log_file = optarg; break;
      case 'd': diff_so_file = optarg; break;
      case 1: img_file = optarg; return 0;
      default:
        printf("Usage: %s [OPTION...] IMAGE [args]\n\n", argv[0]);
        printf("\t-b,--batch              run with batch mode\n");
        printf("\t-l,--log=FILE           output log to FILE\n");
        printf("\t-d,--diff=REF_SO        run DiffTest with reference REF_SO\n");
        printf("\t-p,--port=PORT          run DiffTest with port PORT\n");
        printf("\n");
        exit(0);
    }
  }
  return 0;
}
```

对于其中出现的函数`getopt_long()`，其功能和使用方法是：

1. 功能：解析命令行选项

2. 函数原型：

   ```C
   int getopt_long(int argc, char * const argv[],
                     const char *optstring,
                     const struct option *longopts, int *longindex);
   ```

   argv：选项元素。以`-`开头，然后紧跟一个选项字符。当重复调用函数`getopt_long`的时候，函数会连续返回选项元素

3. 对于里面的`option`结构体的`table`，是选项表，其实现方法为

   ```C
   struct option {
       const char *name;
       int         has_arg;
       int        *flag;
       int         val;
   };
   ```

   name是选项的名称；

   has_arg：

   ​	若为0或者`no_argment`，则不需要参数。

   ​	若为1或者`required_argument`，则需要参数

   flag： 如果为 NULL，返回 val。否则，将 val 存储到 flag 指向的位置，并返回 0

   val：如果 flag 为 NULL，则返回该值，否则存储在 flag 指向的变量中

4. **optstring**

   - 包含了合法的option characters。该字符前包含一个破折号`-`。
   
   - 如果选项字符后面包含一个冒号，则说明这个选项需要一个参数。此时，函数会将一个字符串指针`optarg`指向这个参数。这个参数既可以与选项字符在同一个元素中，例如`-oarg`；也可以与选项分开存放，例如`-o arg`

   - 如果选项后面包含两个冒号。表示这个选项可以有一个可选的参数。此时假设`optstring`是 `o::`，表示 `-o` 选项可以带有可选参数。

     - 如果选项和参数是在**同一个单词**中的`if there is text in the current argv-element `，例如命令行参数是 `-oarg`，则 `optarg` 将会被设置为 `"arg"`。
   
     - 如果命令行参数是 `-o`，则 `optarg` 将被设置为 `NULL`，因为没有在同一个单词内附加参数。
     
   - 函数在浏览参数的时候，默认会将所有的非选项元素（不以`-`开头的参数）放在末尾。但是有两种浏览选项的模式也可以执行：
   
     - 如果`optstring`开头是`+`，则在遇到第一个非选项的时候，会停止
     
     - 如果`optstring`开头是`-`，则每一个非选项参数元素，都被对应为参数1
     
       在这里，函数`getopt_long`的`optstring`开头是`-`，所以如果输入非选项元素（不带`-`的参数），则会将此参数赋值给`img_file`，然后立即结束选项解析。
   
5. 返回值

   当flag是NULL的时候，返回option结构体中的val；否则返回0。

代码整体剖析：

1. 选项表的定义

   ```c
   const struct option table[] = {
     {"batch"    , no_argument      , NULL, 'b'},
     {"log"      , required_argument, NULL, 'l'},
     {"diff"     , required_argument, NULL, 'd'},
     {"port"     , required_argument, NULL, 'p'},
     {"help"     , no_argument      , NULL, 'h'},
     {0          , 0                , NULL,  0 },
   };
   
   ```

   这里定义了一个选项表，包含了程序支持的命令行选项：

   - `--batch` (`-b`): 无需参数，设置程序为批处理模式。
   - `--log=FILE` (`-l`): 需要一个参数，指定日志文件。
   - `--diff=REF_SO` (`-d`): 需要一个参数，指定参考差异文件。
   - `--port=PORT` (`-p`): 需要一个参数，指定端口号。
   - `--help` (`-h`): 无需参数，显示帮助信息。

2. 解析选项

   ```c
   while ( (o = getopt_long(argc, argv, "-bhl:d:p:", table, NULL)) != -1) {
   ```

   使用 `getopt_long` 来解析命令行参数。如果找到选项，`o` 将会被赋值为该选项的对应字符。

3. 处理选项

   ```c
   switch (o) {
     case 'b': sdb_set_batch_mode(); break;
     case 'p': sscanf(optarg, "%d", &difftest_port); break;
     case 'l': log_file = optarg; break;
     case 'd': diff_so_file = optarg; break;
     case 1: img_file = optarg; return 0;
     default:
       // 显示用法信息
   }
   
   ```

   - 对于 -b 选项，调用 sdb_set_batch_mode() 函数。
   - 对于 -p 选项，使用 sscanf 将参数转换为整数并存储到 difftest_port。
   - 对于 -l 和 -d 选项，将相应的参数存储到 log_file 和 diff_so_file 变量中。
   - 对于 1，这表示一个图像文件参数，存储在 img_file 中并返回 0。
   - 如果遇到未知选项，则显示用法信息并退出程序。

4. 返回值

   函数返回0，表示解析成功。

**init_rand()**

在`src/utils/timer.c`中：

```c
void init_rand() {
  srand(get_time_internal());
}
```

`get_time_internal()`函数根据宏定义，来确定一个内部的时间。而里面的`srand()`函数是配合伪随机数函数`rand()`存在的：

- `rand()`函数：生成一个伪随机数
- `srand(seed)`：为伪随机数函数生成了一个以`seed`作为起点的随机序列，但是这个序列是与seed值关联的。相同的seed值，调用`srand`会产生相同的序列。在你重新设置相同的种子值后，伪随机数生成器会从相同的初始状态开始，生成的随机数序列也会完全相同。

所以这里函数`init_rand`的意思是，根据当前内部的时间（变量），生成一个序列。这样每次调用`rand()`函数的时候，都会产生不同的值。

**void init_log(const char *log_file)**

```C
FILE *log_fp = NULL;

void init_log(const char *log_file) {
  log_fp = stdout;	//log_fp 指向标准输出
  if (log_file != NULL) {
    FILE *fp = fopen(log_file, "w");
    Assert(fp, "Can not open '%s'", log_file);
    log_fp = fp;
  }
  Log("Log is written to %s", log_file ? log_file : "stdout");
}
```

这个函数设置了日志记录的输出位置，优先考虑用户指定的文件名。如果无法打开指定的文件，则默认将日志输出到标准输出。通过这种方式，可以灵活地控制日志输出的目标。

假设你调用 `init_log("mylog.txt");`：

- `log_fp` 将指向 `mylog.txt` 文件。
- 如果 `mylog.txt` 文件无法打开，程序将终止并显示错误信息。
- `Log` 函数将输出 "Log is written to mylog.txt"。

如果你调用 `init_log(NULL);`：

- `log_fp` 将指向标准输出 `stdout`。
- `Log` 函数将输出 "Log is written to stdout"。

**void init_mem()**

```c
static uint8_t *pmem = NULL;
void init_mem() {
#if   defined(CONFIG_PMEM_MALLOC)
  pmem = malloc(CONFIG_MSIZE);
  assert(pmem);
#endif
  IFDEF(CONFIG_MEM_RANDOM, memset(pmem, rand(), CONFIG_MSIZE));
  Log("physical memory area [" FMT_PADDR ", " FMT_PADDR "]", PMEM_LEFT, PMEM_RIGHT);
}
```

给内存`pmem`分配空间。

如果定义了`CONFIG_PMEM_MALLOC`宏，则分配字节大小为`CONFIG_MSIZE`的空间给`pmem`

如果定义了`CONFIG_MEM_RANDOM` 宏，则将`pmem`指向的内存区域，填充为随机值（跟前面的`init_rand()`有关系）。`rand()` 生成一个随机值，`CONFIG_MSIZE` 是内存区域的大小。

**void init_isa()**

定义在`nemu/src/isa/riscv32/init.c`

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

static void restart() {
  /* Set the initial program counter. */
  cpu.pc = RESET_VECTOR;

  /* The zero register is always 0. */
  cpu.gpr[0] = 0;
}

void init_isa() {
  /* Load built-in image. */
  memcpy(guest_to_host(RESET_VECTOR), img, sizeof(img));

  /* Initialize this virtual computer system. */
  restart();
}
```

:computer:可以看出客户程序`img`是一个基于risv32的指令数组。实现的功能是在`pc+16`的位置，存储数据0；并将`pc+16`内存地址处的数据（0）存放到寄存器`a0`中。

`void init_isa()`的逻辑是，首先将内置的程序存放到内存指定区域：

```C
/* Load built-in image. */
memcpy(guest_to_host(RESET_VECTOR), img, sizeof(img));
```

此内存地址为`guest_to_host(RESET_VECTOR)`，一个固定的内存位置`RESET_VECTOR`。对应的函数实现为(`src/memory/paddr.c`)：

```C
static uint8_t *pmem = NULL;

uint8_t* guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
```

`pmem`是一个指向128MB的物理内存指针，这个paddr是未来才会用到的物理地址，现在不必深究。

输入的`RESET_VECTOR`和对应`CONFIG_MBASE`的定义分别是：

```c
#define CONFIG_MSIZE 0x8000000	
#define CONFIG_PC_RESET_OFFSET 0x0 


#define PMEM_LEFT  ((paddr_t)CONFIG_MBASE)					// 0x80000000
#define PMEM_RIGHT ((paddr_t)CONFIG_MBASE + CONFIG_MSIZE - 1)
#define RESET_VECTOR (PMEM_LEFT + CONFIG_PC_RESET_OFFSET)	//0x80000000
```

所以`guest_to_host(RESET_VECTOR)`会返回一个指向内存地址偏移量为0的位置，即为`pmem[0]`。

函数`guest_to_host()`的地址映射：将CPU要访问的物理内存地址，**映射**到`pmem`中相应偏移位置。

好的，我们现在可以总结下`init_isa()`的第一步做了什么：

将内置程序存放到NEMU的内存地址偏移量为0的位置。（对应的客户物理内存地址为`0x80000000`）

然后我们再看初始化虚拟计算机系统的操作`static void restart()`

```C
static void restart() {
  /* Set the initial program counter. */
  cpu.pc = RESET_VECTOR;

  /* The zero register is always 0. */
  cpu.gpr[0] = 0;
}
```

当 `static` 修饰一个函数时，该函数的作用域限制在定义它的文件内。也就是说，这个函数只能在定义它的源文件中调用，其他源文件无法访问。通过这种方式，函数可以避免与其他文件中的同名函数冲突，增强封装性。

函数实现的功能，就是将客户机的`pc`设置为物理地址`0x8000000`，这样对应NEMU的内存地址是偏移量为0的位置，即为上文中保存客户程序的位置。并且将0寄存器设置永远为0。

这样`init_isa()`的结果就是：

首先将内置的（`bulit-in`）客户程序读取到内存偏移为0的地方，然后将cpu的`pc`指向这个程序的初始地址。

**load_img()**

当初始化ISA后，下一步就是将客户程序读取到内存中。

```C
static char *img_file = NULL;

static long load_img() {
  if (img_file == NULL) {
    Log("No image is given. Use the default build-in image.");
    return 4096; // built-in image size
  }

  FILE *fp = fopen(img_file, "rb");
  Assert(fp, "Can not open '%s'", img_file);

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);

  Log("The image is %s, size = %ld", img_file, size);

  fseek(fp, 0, SEEK_SET);
  int ret = fread(guest_to_host(RESET_VECTOR), size, 1, fp);
  assert(ret == 1);

  fclose(fp);
  return size;
}
```

里面涉及到的几个函数简介：

- `fopen(img_file, "rb")`：使用 `fopen` 函数以二进制模式 (`"rb"`) 打开指定的镜像文件

- 获取文件大小：使用 `fopen` 函数以二进制模式 (`"rb"`) 打开指定的镜像文件；`fseek(fp, 0, SEEK_END)`将文件指针移到文件末尾，随后使用 `ftell(fp)` 获取当前文件指针的位置，从而得到文件的大小。

- 重置文件指针： `fseek(fp, 0, SEEK_SET)` 将文件指针重置到文件开头，以便后续读取文件内容。

- 读取文件内容到内存

  `size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)`。

  使用 `fread` 将文件内容读取到指定的内存地址，这里使用 `guest_to_host(RESET_VECTOR)` 计算目标地址。

NEMU执行的客户程序`img_file`，来源有两个：

- 运行NEMU时输入的客户程序文件`img_file = optarg; return 0;`：`parse_args()`在解析命令行的时候，如果输入非选项参数——客户程序文件，则将`img_file`的值置为输入的参数，然后立即终止解析参数并且返回0
- 内置的客户程序

所以运行NEMU的时候，如果没有指定客户程序文件，则会执行内置的客户程序。

如果指定了客户程序文件，则会获取此客户程序并且将此程序加载到上文相同的内存位置`0x80000000`即`pmem[0]`，并且返回这个程序的大小。

ok，Monitor的初始化工作结束！:watermelon:它的功能就是设置好ISA和默认程序，初始化内存和cpu状态。

### 运行第一个客户程序

Monitor的初始化工作结束后, `main()`函数会继续调用`engine_start()`函数 (在`nemu/src/engine/interpreter/init.c`中定义)来实现与用户的命令交互:computer_mouse:

```C
void sdb_mainloop();

void engine_start() {
#ifdef CONFIG_TARGET_AM
  cpu_exec(-1);
#else
  /* Receive commands from user. */
  sdb_mainloop();
#endif
}
```

查看函数`sdb_mainloop()` (在`nemu/src/monitor/sdb/sdb.c`中定义)

```C
static int is_batch_mode = false;

void sdb_mainloop() {
  // 如果是批处理模式，则执行完毕，立刻终止简易调试器(Simple Debugger)的主循环
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }
  // 从输入获取命令和参数
  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);	// 终止符 '\0'

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");	// 将str的第一个空格之前的字符作为 命令
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;	// 空格后的字符串作为 参数
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }	// 执行命令
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}
```

里面涉及到的难以理解的地方，我们分析下：

- strtok()

#### strtok()

**STFSC**：`man 3 strtok`

```C
#include <string.h>
char *strtok(char *str, const char *delim);
```

- `strtok`是一个按照分隔符`delim`将字符串`str`分割的函数

- 第一次调用时候，需要指定解析字符串`str`；如果后面想继续解析字符串`str`，这个`str`就必须是`NULL`

- 扫描字符串若发现有分隔符集合`delim`或者空字节`'\0'`，就会将其统一覆盖成字符串终止的空字节`'\0'`。（:warning: 这样就会修改原字符串）

- 每次调用`strtok()`，都会返回指向下一个token字符串的指针（ 此token带有终止符号`'\0'`，但是不包括分隔符`delim`，因为此时的分隔符已经被终止符号替代）。

- 对同一个字符串`str`进行连续调用`strtok()`，这时候函数会维护一个函数指针，这个指针决定了指向下一个`token`的起始位置。函数指针确保了每次调用`strtok()`的时候，都会从上一次拆分结束的起始位置开始查找下一个token，而不是从头开始。

  第一次调用的时候，函数指针会指向字符串`str`的第一个字节。

- 处理字符串`str`中分隔符的思路（确保token只能为非空字符串）：

  - 开头和结尾的分隔符会被忽略
  - 连续多个分隔符会被视为单个分隔符处理

写一个小程序来实践下

```C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char str[] = "1a/bbb///:/;:/cc;xxx:yyy:";

    char *token = strtok(str, ":/;");

    while(token != NULL) {
        printf("token is %s\n", token);
        token = strtok(NULL, ":/;");
    }
    printf("str = %s\n", str);
    return 0;
}
```

输出为

```tex
token is 1a
token is bbb
token is cc
token is xxx
token is yyy
str = 1a
```

- 分隔符集合`dlim`包含了许多分割字符，对于待解析的字符串，遇到集合中的任意一个字符都要被分割。
- 第一次调用`strtok()` 时，函数内部会保存一个指向该字符串中下一个标记位置的指针，以供后续调用。
- 第一次调用`strtok()`时，会将`str`分割为第一个token字符串`la`。
- 后续要继续解析这个字符串时，需要将`str`替换为`NULL`，这样再次调用时，就会从上一次解析完毕的地方开始再新一轮的解析。
- 替换后，str字符串中的分隔符会被终止空字符`\0`替换掉

这样`sdb_mainloop()`的作用， 是对客户计算机的运行状态进行监控和调试。

**模拟cpu执行**

```C
// 译码相关代码
typedef struct Decode {
  vaddr_t pc;
  vaddr_t snpc; // static next pc
  vaddr_t dnpc; // dynamic next pc
  ISADecodeInfo isa;
  IFDEF(CONFIG_ITRACE, char logbuf[128]);
} Decode;


static void exec_once(Decode *s, vaddr_t pc) {
  s->pc = pc;
  s->snpc = pc;
  isa_exec_once(s);
  cpu.pc = s->dnpc;
#ifdef CONFIG_ITRACE
  char *p = s->logbuf;
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);	//FMT_WORD : "0x%08x"
  int ilen = s->snpc - s->pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst.val;
  for (i = ilen - 1; i >= 0; i --) {
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0) space_len = 0;
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;

#ifndef CONFIG_ISA_loongarch32r
  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  disassemble(p, s->logbuf + sizeof(s->logbuf) - p,
      MUXDEF(CONFIG_ISA_x86, s->snpc, s->pc), (uint8_t *)&s->isa.inst.val, ilen);
#else
  p[0] = '\0'; // the upstream llvm does not support loongarch32r
#endif
#endif
}
```

- 函数`snprintf()`用法:book:

  ```C
  int snprintf(char *str, size_t size, const char *format, ...);
  ```

  **参数说明**

  - **`str`**: 指向目标缓冲区的指针，用于存储生成的格式化字符串。
  - **`size`**: 目标缓冲区的大小，`snprintf` 将最多写入 `size - 1` 个字符，并自动在最后添加一个空字符 `'\0'`。
  - **`format`**: 格式化字符串，与 `printf` 的格式化字符串相同。
  - **`...`**: 可变参数，用于指定格式化字符串中的变量。

  **返回值**

  如果成功，`snprintf` 返回要写入的字符串长度（不包括终止的空字符）。

  如果返回值大于或等于 `size`，则表示输出被截断，你可能需要更大的缓冲区。

这里分析

```C
  char *p = s->logbuf;
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);//FMT_WORD : "0x%08x"
```

将当前程序计数器`s->pc`的值(内存地址)，以`"0x%08x"`的格式保存到指针`p`指向的`logbuf[128]`缓冲区中。

举例：

假设 `s->pc` 的值为 `0x80000000`，且 `FMT_WORD` 展开为 `0x%08x`：

- 执行 `snprintf(p, sizeof(s->logbuf), "0x%08x:", s->pc);` 之后，`p` 中的内容将是 `"0x80000000:"`。
- `p` 现在将指向 `"0x80000000:"` 后的下一个位置，以便你在后续操作中继续向缓冲区中追加内容。

下面就是从对应的地址处读取数据

```C
int ilen = s->snpc - s->pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst.val;
  for (i = ilen - 1; i >= 0; i --) {
    p += snprintf(p, 4, " %02x", inst[i]);
  }
```

将当前`pc`所指地址处的数据，以4个字节为一组，按照`%02x`的格式保存到指针`p`中。

因为现在对于Decode的结构不是很了解，所以这里`inst`的值就不必去深究怎么获得的了。后面的内容，也等到后续学习了关于译码的相关问题再来解决。

#### 优美地退出

`make run`启动nemu后直接输入`q`退出，得到如下最后一行的错误:

```shell
Welcome to riscv32-NEMU!
For help, type "help"
(nemu) q
make: *** [/home/crx/study/ics2023/nemu/scripts/native.mk:38: run] Error 1
```

这里因为调用q的函数`cmd_q`的时候，返回了一个`-1`，结果报错。所以我们需要考虑主函数`main()`在调用完`engine_start()`后，对于其返回值的处理。

我们先回顾下，nemu的主函数`nemu-main.c`：

```C
#include <common.h>

void init_monitor(int, char *[]);
void am_init_monitor();
void engine_start();
int is_exit_status_bad();

int main(int argc, char *argv[]) {
  /* Initialize the monitor. */
#ifdef CONFIG_TARGET_AM
  am_init_monitor();
#else
  init_monitor(argc, argv);
#endif

  /* Start engine. */
  engine_start();

  return is_exit_status_bad();
}
```

可以看出，初始化监视器并且程序开始运行后，最终函数的返回语句为`return is_exit_status_bad()`.

所以我们看下这个返回值函数`is_exit_status_bad`的源码：

```C
#include <utils.h>

NEMUState nemu_state = { .state = NEMU_STOP };

int is_exit_status_bad() {
  int good = (nemu_state.state == NEMU_END && nemu_state.halt_ret == 0) ||
    (nemu_state.state == NEMU_QUIT);
  return !good;
}
```

可以看出要修改的地方就是函数`cmd_q`：

```C
static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}
```

这样就可以解决报错问题了。

## 基础设施

### 单步执行

描述：让程序单步执行`N`条指令后暂停执行，当`N`没有给出时, 缺省为`1`

格式：si [N]

举例：si 10

代码实现：

```c
uint64_t get_steps_from_args() {
  char *num = strtok(NULL, " ");

  /* no argument given */
  if (num == NULL) {
    return 1;
  }

  uint64_t result = 0;
  int i = 0;

  /* Loop through the string*/
  while(num[i] != '\0') {
    // Check if the character is a digit (0-9)
    if (num[i] >= '0' && num[i] <= '9') {
      result = result * 10 + (num[i] - '0');
    } else {
      //Return 1 if a non-digit character is found
      return 1;
    }

    i++;
  }

  return result;
}


static int cmd_step(char *args) {
  /* get the num of steps from args*/
  uint64_t n = get_steps_from_args();
  cpu_exec(n);
  return 0;
}
```

这里用到了函数`get_steps_from_args`来处理参数：

- 如果输入非法字符，则返回1
- 如果输入为`NULL`，则返回1
- 输入为数字字符串，将其转换为对应的数字，并返回

这个函数的有意思的两点：

- `strtok()`函数：第一次调用处在函数`sdb_mainlooop()`处

  ```C
  char *cmd = strtok(str, " ");
  char *args = cmd + strlen(cmd) + 1;
  ```

  此时调用完函数`strtok()`后，若想继续解析后面的符号(token)，则直接使用

  ```C
  char *num = strtok(NULL, " ");
  ```

- 将数字字符串转换为对应的10进制数据

### 打印寄存器

描述：打印寄存器状态

格式：info SUBCMD

举例：info r

执行`info r`之后, 就调用`isa_reg_display()`, 在里面直接通过`printf()`输出所有寄存器的值即可.

代码已经准备了如下的API:

```C
// nemu/src/isa/$ISA/reg.c
void isa_reg_display(void);
```

:qatar:

- risv32寄存器在哪里定义

  > 在`nemu/src/isa/risv32/reg.c`中被定义为字符串数组

- risv32寄存器的值是怎么修改和使用的？

  > 这时需要回忆之前`iniy_isa()`的相关功能：第一步，将内置的客户程序读入到内存中；第二步就是初始化寄存器。
  >
  > 例如将0号寄存器置为存放0的状态：
  >
  > ```C
  > cpu.gpr[0] = 0;
  > ```

- 如何获取risv32的寄存器的值？

  > 遍历`cpu`中寄存器值即可
  >
  > 寄存器结构体`CPU_state`的定义放在`nemu/src/isa/$ISA/include/isa-def.h`中, 并在`nemu/src/cpu/cpu-exec.c`中定义一个全局变量`cpu`.
  >
  > 查看`CPU_state`：
  >
  > ```c
  > typedef struct {
  >   word_t gpr[MUXDEF(CONFIG_RVE, 16, 32)];
  >   vaddr_t pc;
  > } MUXDEF(CONFIG_RV64, riscv64_CPU_state, riscv32_CPU_state);
  > ```
  >
  > 此问题可转换为：遍历`cpu`结构体中寄存器`gpr`和程序指针`pc`的值

首先实现risv32的`isa_reg_display`API:

```C
#define gpr(idx) (cpu.gpr[check_reg_idx(idx)])
void isa_reg_display() {
  for(int i = 0; i < MUXDEF(CONFIG_RVE, 16, 32); i++) {
    if (gpr(i) == 0) {
      printf("%-3s = %d\n", regs[i], gpr(i));
    }
    else {
      printf("%-3s = 0x%08x\n", regs[i], gpr(i));
    }
  }
}
```

然后再从`sdb.c`中调用此函数

```C
static int cmd_info(char *args) {
  char *arg = strtok(NULL, " ");

  if (arg == NULL || strcmp(arg, "r") == 0) {
    isa_reg_display();
  }
  return 0;
}
```

运行`nemu`后，输入`c`执行完毕内置程序，此时打印寄存器的值：

```bash
(nemu) info
$0  = 0
ra  = 0
sp  = 0
gp  = 0
tp  = 0
t0  = 0x80000000
t1  = 0
t2  = 0
...
```

> [!IMPORTANT] 
>
> 为什么执行内置程序后，寄存器`t0`被设置为`0x80000000`了？
>
> 这里就需要结合risv32的指令集来解释了
>
> ```c
> static const uint32_t img [] = {
> 0x00000297,  // auipc t0,0		-- 将t0的值设置为当前pc(0x80000000)的值 (Add Upper Immediate to PC)
> 0x00028823,  // sb  zero,16(t0) 
> 0x0102c503,  // lbu a0,16(t0)	
> 0x00100073,  // ebreak (used as nemu_trap)
> 0xdeadbeef,  // some data
> };
> ```
>
> 这样就可以解释为何执行完毕内置程序后，寄存器的值为`0x80000000`了。

### 扫描内存

描述：输入起始内存地址，以十六进制行书输出连续的`N`个4字节。

格式：x N addr

举例：x 10 0x80000000

分析：

上面分析函数`init_mem()`和`init_isa()`时，可以得出几个结论：

1. 客户程序的物理地址，将会被映射到模拟出的内存数组`pmem`中

2. 物理地址到内存数组`pmem`的映射规则是

   ```C
   uint8_t* guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
   ```

而`paddr_t`的定义（`include/common.h`）

```c
typedef MUXDEF(PMEM64, uint64_t, uint32_t) paddr_t;
```

这里`paddr_t`代指物理地址，而在`risv32`系统上，相当于`unit32_t`。

思路：

- **地址转换**：输入物理内存地址`paddr`，通过函数`guest_to_host()`找到对应的数组地址
- **N**：根据`gdb`输出内存地址的惯例，输出数据每4个字节占一行

实现：

在调用函数`vaddr_read()`来获取内存数据时，需要将对应的头文件`/include/memory/paddr.h`包含进来。

```C
#include "memory/vaddr.h"

static int string_to_num(char *str) {
  int i = 0;
  int result = 0;
  /* Loop through the string*/
  while(str[i] != '\0') {
    // Check if the character is a digit (0-9)
    if (str[i] >= '0' && str[i] <= '9') {
      result = result * 10 + (str[i] - '0');
    } else {
      printf("Warning: %s is not a num!\n", str);
      return 0;
    }

    i++;
  }
  return result;
}

vaddr_t hex_string_to_vaddr(const char* hex_str) {
  vaddr_t result = 0;
  sscanf(hex_str, "%x", &result);
  return result;
}


static int cmd_scan_memory(char *args) {
  char *lines_str = strtok(NULL, " ");
  if (lines_str == NULL) {
    printf("Please input N!\n");
    return 0;
  }

  char *addr_str = strtok(NULL, " ");
  if (addr_str == NULL) {
    printf("Please input address!\n");
    return 0;
  }

  int lines = string_to_num(lines_str); //1,2,3...N
  vaddr_t addr_hex = hex_string_to_vaddr(addr_str);

  if (addr_hex < CONFIG_MBASE) {
    printf("Invalid address!\n");
    return 0;
  }

  for (int i = 0; i < lines; i++) {
    printf(FMT_WORD ":", addr_hex + i*4);
    for (int j = 0; j < 4; j++) {
      uint8_t data = vaddr_read(addr_hex + 4*i + j, 1);
      printf(" %02x", data);
    }
    printf("\n");
  }
  return 0;
}
```

## 表达式求值

识别十进制的算数表达式的正则表达式：

- 十进制整数：`[0-9]+`
- `+`, `-`, `*`, `/`：`[+\-*/]`
- `(`, `)`：`[()]`
- 空格串：` +`

:warning:在这里，正则表达式还要作为C语言的字符串进行操作，所以还要符合C语言的转义字符。

例如正则表达式对于匹配一个`+`符号的表达式为：`\+`；但是由于要作为c语言的字符串储存，所以需要写为`\\+`l来抵消特殊字符`\`的特定功能。

### POSIX regex functions

编译正择表达式的函数：`regcomp`

匹配正则表达式函数：`regexec`

函数原型

```C
#include <regex.h>
typedef struct {
               regoff_t rm_so;
               regoff_t rm_eo;
           } regmatch_t;


int regcomp(regex_t *preg, const char *regex, int cflags);

int regexec(const regex_t *preg, const char *string, size_t nmatch,
                   regmatch_t pmatch[], int eflags);

```

函数功能：

- `regcomp()`：将正则表达式`regex`(例如`\d+`匹配任意数量的数字)编译，用于后面的函数`regexec()`做匹配。`preg`是一个指向正则表达式缓冲区模式存储区域的指针。

- `regexec()`：通过编译好的正则表达式模式缓冲区已存在的范例，对不含终止符`\0`的字符串`string`进行匹配。

  `nmatch` ：`pmatch` 数组中的元素个数，即你希望捕获的匹配组数。

  `pmatch`：用于存储匹配位置的数组

  返回值为0，则匹配成功。

- 用于存储匹配位置的数组`regmatch_t`:

  - **`rm_so`**: 匹配的起始位置（下标）
  - **`rm_eo`**: 匹配的结束位置（下标，超出最后一个匹配字符的位置）

遇到的问题：

**POSIX元字符**

因为我在写关于数字的正则表达式时候，采用了并不是POSIX模式匹配的元字符`\\d+`，导致一直编译不过。

后来查询了POSIX的支持的元字符，发现只能用`[0-9]+`来匹配十进制数字。

**`strncpy()`**

函数`strncpy()`

```C
char *strncpy(char *dest, const char *src, size_t n);
// simple implementation
char *
   strncpy(char *dest, const char *src, size_t n)
   {
       size_t i;

       for (i = 0; i < n && src[i] != '\0'; i++)
           dest[i] = src[i];
       for ( ; i < n; i++)
           dest[i] = '\0';

       return dest;
   }
```

用这个函数是想控制输入的字符串长度。

如果`dest`为`12345`，`src`为`+-`，`n`为`2`，则函数调用完毕后，`dest`的值为`+-345`

### 递归求值

递归求值的逻辑

1. 根据token来分析表达式属于BNF中的哪一种情况
2. 求值

待解决问题

- 检查左右括号是否匹配：`static bool check_parentheses(p, q)`
- 确定表达式类型：寻找主运算符，将长表达式分裂为两个子表达式
  - 只有`[+\-*/]`才可能是主运算符
  - 主运算符的优先级在表达式中的优先级是最低的
  - 多个运算符优先级都是最低，根据结合性，最后被结合的运算符才是主运算符
- 错误处理：表达式不合法时候的错误处理

函数描述

- 检查表达式左右括号是否匹配`static bool check_parentheses(p, q)`：
  - 输入：`p`和`q`分别指示这个表达式的开始位置和结束位置
  - 输出：若表达式被一对匹配括号包围，同时括号内部的左右括号也均匹配，则输出`true`；否则输出`false`
  - 逻辑：
    - 第一步，检查`p`和`q`位置是否分别为`(`和`)`。如果是，则取出括号内部的子表达式进行第二步判断；如果不是，则记录`false`
    - 第二步，检查括号内部的子表达式中，左右括号是否全部匹配。如果表达式中左右括号全部匹配，则返回`true`，否则返回`false`（例如`(1+2)-(3+4)`，将会在第二步中取得子表达式`1+2)-(3+4`，证明不符合要求）
  - 注意：此方法仅用来判断一个表达式是否被括号包围。如果是则拆除左右括号进行下一步运算，如果不是则交给其他步骤处理。至于拆除后的括号是否为正确的表达式，不在此方法讨论。
- 确定表达式类型
  - 判断是否为括号匹配表达式：`assert_expression(p,q)`
    - 如果表达式中的左右括号不能全部匹配，则`assert(0)`
  - 确定主运算符
    - 如果运算符被`()`包围，则不记录此运算符
    - 如果下一个运算符是`+`或`-`，则将记录的运算符替换为为下一个运算符
    - 如果下一个运算符是`*`或`/`，且记录的运算符也是`*`或`/`，则将下一个运算符赋给记录的运算符

在这里其实我还是对“如何保证该表达式合法”有疑惑，所以翻阅了下cs61a中关于[scheme实现表达式求值](https://www.composingprograms.com/pages/34-interpreters-for-languages-with-combination.html)的资料，看看能否解决这个疑惑。

- 将表达式看做数据！（将表达式转化为tokens数组）

- 表达式解析：包含了词法分析：将输入的字符串分解为最小的语法单元tokens（类似num或者`+-*/`）;和语法分析：将tokens构建为表达式树

  词法分析会产生tokens序列（函数`make_tokens()`）

  语法分析会消耗掉tokens序列（这里好像并没有将其转换为表达式的方法，而是将表达式的判断放入了计算函数`expr()`中）

- 语法分析是树递归函数，按照所有大的表达式可以被分解为小的表达式。递归产生的结构化表达式会被**计算器**消耗掉

- 计算器：输入的表达式两个合法的语法格式就是数字和调用表达式（括号包围的表达式）

  数字计算：直接返回本身值

  调用表达式：需要函数来处理

那么现在思考“如何计算表达式的值”这个问题：

- 如果表达式是一个数字，那么自我运算返回数字本身
- 如果表达式是一个括号包围的表达式，则计算括号内部的表达式
- 如果是一个表达式，则计算表达式的值

实现这些功能后，现在还有一个负数求值的功能需要实现，例如计算

```tex
"-1 + 1"
"1 + -1"
"--1"    /* 我们不实现自减运算, 这里应该解释成 -(-1) = 1 */
```

考虑两个问题：

- 负号和减号都是`-`, 如何区分它们?
- 负号是个单目运算符, 分裂的时候需要注意什么?

我的回答：

- 判断`-`前面是不是另一个表达式即可。如果`-`前面是一个表达式，那么`-`为减号；若前面不是一个表达式（运算符），则`-`为符号（例如`-1`主运算符为`-`，但是前面不是一个表达式，则`-`为负数）
- `--1`的主运算符是第二个`-`，而主运算符的前一个表达式不是一个表达式（是一个运算符`-`）

既然要对`-`前面的表达式进行判断，从而决定是负号还是减号。所以我们需要对求值的`val1 = eval(p, op - 1)`进行处理：

```C
  if (p > q) {
    /* Bad expression */
    if(tokens[p].type == '-'){
	return 0;
	}
  }
```

这样做的结果就是，对于类似`-3`的负数，确定好主运算符`-`号后，立刻判断`-`前面并不是表达式，所以就相当于将其转换成`0-3`的场景。

对于`--1`的处理方法，可以从确定主运算符的方法`locate_main_operator()`入手。将`--x`其转换为`-(expr)`的样式即可。这样就可以用上面的方式，将表达式进一步转换为`0-(expe)，`其中`expr=-x`。

简单来说，就是将两个连续的`-`运算符的场景，在判断主运算符函数中，单独归为一类。

### 表达式生成器

设计生成表达式的函数`gen_rand_expr()`：输出合法的表达式，并将表达式保存到缓冲区`buf`中

```C
void gen_rand_expr() {
  switch (choose(3)) {
    case 0: gen_num(); break;
    case 1: gen('('); gen_rand_expr(); gen(')'); break;
    default: gen_rand_expr(); gen_rand_op(); gen_rand_expr(); break;
  }
}
```

其中涉及到四个函数：

- 随机整数生成函数`gen_num()`
- 生成指定字符函数`gen(char)`
- 生成随机运算符函数`gen_rand_op()`
- 生成一个小于`n`的随机数函数`uint32_t choose(uint32_t n)`

前三个函数均将生成的结果，存放到`buf`中。

实现完毕这几个函数后，编译运行`nemu/tools/gen-expr/gen-expr.c`即可实现生成表达式和结果。

结果出现了

```C
 unsigned result = (101)((11)-291)+((241))/(540)*(116)/(150)-(990-208/368/549-((70))+(64))*566;
```

即`(expr1)(expr2)`的错误场景。分析是因为在每一次生成表达式的时候，都会用`concat()`函数附在已存在的表达式上，这里第一个`(expr1)`就是第一次生成的表达式，而第二次生成的表达式`(expr2)`则由于没有清空`buf[]`导致直接附在了表达式1`(expr1)`的后面导致了错误。

所以这里的解决方法就是在每一次生成表达式之前，将`buf`置空（我这里是直接将首字符设置为`\0`的方法）

但是仍有几个问题需要考虑：

- 如何生成长表达式, 同时不会使`buf`溢出?

  答：在`buf`中添加任何数据之前，需要判断新添数据和原有数据的长度之和，是否超出`buf`的限制长度。如果超出，则不进行添加数据。

- 如果生成的表达式有除0行为, 你编写的表达式生成器的行为又会怎么样呢?

  答：会提示错误` warning: division by zero [-Wdiv-by-zero]	`

- 如何过滤求值过程中有除0行为的表达式?

  答：~~暂时不考虑了。~~因为后续测试中，会在除以0的情况报错，所以这里简单地处理下：如果运算符是除号`/`，则将后面的表达式强制改为一个非零的数字。

#### 遇到的问题

但是计算的时候，遇到这样的计算还是会出问题：

> 例如负溢出：`0 - 19 = 4294967277`。这样的原因是由于计算结果均由`uint32_t`的32位无符号整型变量保存，数字`4294967277`的十六进制为`0xFFFF FFED`，加上`19(0x13)`正好是`0x1 0000 0000`超出了`uint32_t`的最大表示范围，所以截取其低32位即为`0`。这样其实并不会产生问题，产生问题的其实是负数的除法场景:
>
> ```
> -19 / 3 = ?
> ```
>
> 在有符号整型`int32_t`运算的时候，计算结果为`-6(0xFFFF FFFA)`;
>
> 如果是先将两个数转换为对应的无符号整数`uint32_t`，然后再进行计算，就会让除法表达式转换为`4,294,967,277 / 3 = 1,431,655,759`。所以需要保证计算的时候，进行有符号的计算。
>
> 解决方法：将计算表达式值的函数`calc_apply()`的输入、输出参数均转化为有符号整型`int32_t`。函数`eval()`中获取计算函数`calc_apply()`的结果时，将其解释为一个`uint32_t`的无符号整型变量即可。这样就不会出现计算负数除法的时候，先将负数转换为一个大的正数，然后做计算的错误场景。
>

还有另一个问题

> `make_tokens()`函数，当字符为空格时，`tokens[]`不会记录。所以数组的长度记录`nr_token`也不能更新，直接略过这个空格场景。

另外一个问题是头文件的路径问题：

> 在A机器上的头文件路径为`/home/crx/study/ics2023/nemu/src/monitor/sdb/sdb.h`，所以我在A机上的C代码中，直接引用对应的头文件
>
> ```C
> #include "/home/crx/study/ics2023/nemu/src/monitor/sdb/sdb.h"
> ```
>
> 然而我在B机器上编译运行的时候，因为这个头文件并不在对应的目录中，所以会报错。
>
> 分析下头文件`sdb.h`固定在项目目录`/src/monitor/sdb/`下面，是可以固定的。而前面的`/home/crx/study/ics2023/nemu/`恰好就是PA0阶段设置的环境变量`$NEMU_HOME`。
>
> 所以解决思路就是，将`/home/crx/study/ics2023/nemu/src/monitor/sdb/`作为头文件的索引地址，在编译的时候作为额外的头文件查找路径即可。
>
> 解决方案：在Makefile中，添加一行指定编译选项
>
> ```makefile
> CFLAGS += -I$(NEMU_HOME)/src/monitor/sdb
> ```
>
> 作用是告诉编译器在编译时查找头文件时，除了默认的系统目录，还要在 `$(NEMU_HOME)/src/monitor/sdb` 目录下查找头文件

## 监视点

需求分析：

- 扩展表达式求值功能
- 实现监视点
- 断点

### 扩展表达式求值功能

表达式求值的最终结果是`uint32_t`类型。

主线任务需要实现：

- 16进制数据的表示
- 打印寄存器的值
- 指针的解引用
- 逻辑运算`==`、`!=`、`&&`

支线任务：

- 修理代码：根据指针的解引用，修改负数的判断规则（难！预计两小时）
- 修改代码：将数字字符串转换为十进制数字的方法，改为用函数`sscanf()`的样式（删除函数`string_to_num`）
- 优化：或许可以`man 3 strtol`来寻求更好地字符串转长数字的方法

第一个十六进制数据的表达式的相关思考。

> 问1：是否可以用正则表达式识别16进制？(`0x12..9ABCDE`）
>
> 分析：`0x`开头，随后是任意数量的十六进制数字（从数字0-9和大小写不区分的A-F）
>
> 得出的结论是：`0x[0-9A-Fa-f]+`
>
> 并且十六进制的正则表达式规则`TK_HEX`，需要放在正则表达式`TK_NUM`的前面，否则正则表达式识别`0x...`的时候，会优先选择数字匹配`0`，然后遇到符号`x`报错。
>
> 问2：识别出此表达式为16进制表达式后，在哪处理此值？
>
> 答：在`eval()`函数的流程`else if(p == q)`，判断表达式为数字（包括10进制和16进制），调用函数将其转换为`uint32_t`的整数数据。
>
> 需要实现一个函数：将数字字符串转换为无符号整型变量
>
> 输入：一个结构体token元素，按照结构体里面的type属性分为10进制字符串和16进制字符串
>
> 处理：按照对应的type类型，判断按照哪种进制处理字符串数据，并且调用函数将字符串转换为无符号整型变量
>
> 输出：无符号整型变量
>
> 附：在实现这个函数的时候，我希望后期维护性提升。例如后面需要支持8进制数据的时候，仅需要简单处理一下，而不用修改大量代码。
>
> 所以用到了函数指针，创建一个函数指针的数组，根据token的type确定对应的函数调用。
>
> 另一个问题：将字符串转为10进制、16进制的方法？
>
> 答：使用`sscanf()`，将16进制字符串转化为数字存储。函数原型如下：
>
> ```C
> int sscanf(const char *str, const char *format, ...);
> ```
>
> - `str`：要扫描的输入字符串。
> - `format`：格式控制字符串，用于指定如何解析输入字符串。
> - `...`：一系列变量的地址，这些变量将接收解析出来的数据。
>
> 返回值：`sscanf` 成功返回转换的项数。如果返回值为1，表示成功从字符串中读取了一个十六进制数

第二个打印寄存器的值，判断依据是表达式以`$`开头。然后获取寄存器的值，以`uint32_t`的形式打印出来。获取寄存器的值，是ISA相关的功能，框架代码给了一个结构：

```C
// nemu/src/isa/$ISA/reg.c
word_t isa_reg_str2val(const char *s, bool *success);
```

它用于返回名字为`s`的寄存器的值, 并设置`success`指示是否成功.

打印寄存器值的相关思考

> 1. 寄存器的正则表达式
>
>    答：以字符`$`开头，后面跟着寄存器的名称，是任意数量的字符和数字的拼装。所以正则表达式为`\$[0-9a-z-A-Z]+`
>
> 2. 寄存器表达式的求值
>
>    答：调用函数`isa_reg_str2val()`来获取返回名字为`s`的寄存器的值。`s`是去掉符号`$`后的寄存器名称字符串。
>
> 3. 为了让代码更简约，能做点什么？
>
>    答：寄存器表达式也是一个单独的token保存：`token.type = TK_REG`，`token.str = $reg_name_string`.
>
>    所以在计算寄存器表达式的时候，希望也可以用统一的函数`convert_token_to_unsigned_num()`来处理代表寄存器表达式的token。
>
>    而框架给的API原型为
>
>    ```C
>    word_t isa_reg_str2val(const char *s, bool *success);
>    ```
>
>    而我想实现的获取寄存器值的函数`get_register_value()`的输入参数仅有代表寄存器名称的`s`。所以需要在函数`get_register_value()`中，首先声明一个布尔变量，然后和变量名一起作为参数调用框架的API。
>
>    然后根据布尔变量的结果，来确定是否返回获取的值。
>
> 4. 怎么实现这个API？
>
>    答：首先找到这个获取寄存器值API的位置`nemu/src/isa/riscv32/reg.c`。步骤是:
>
>    - 根据寄存器名称，获取寄存器索引`reg_idx_by_name()`
>    - 打印寄存器的值
>
>    在`risv-32`的体系下，cpu寄存器是一个大小为32的数组。现在已经在对应`risv-32`的体系（`nemu/src/isa/riscv32/local-include/reg.h`）下，实现了根据索引（0-31）来获取寄存器值的方法`gpr(idx)`（`nemu/src/isa/riscv32/local-include/reg.h`）。
>
>    所以我们可以实现”根据寄存器的名称获取寄存器编号“的方法，来间接获取寄存器的值。我们暂且将这个方法命名为`static int reg_idx_by_name(const char *name)`

第三个指针的解引用，判断依据是表达式以`*`开头，并且不是乘号的作用。

在表达式求值之前的`expr()`流程中，提前将`*`代表的运算类型确定好。这样就会简化求值函数`eval()`的处理复杂度：仅仅关注于计算，而不是处理运算符。

但是我在判断`-`的类型时，并没有在`expr()`中实现这个功能，反而是在寻找主运算符的函数`locate_main_operator()`和`eval()`中通过一些trick，实现了负数的运算。

这里先按照PA手册实现解引用的判断，写完后，负数的运算再作为支线任务解决。

随后再调用函数`eval()`来处理对应表达式的值。

指针解引用的思考：

> 1. 如何判断`*`是乘法还是解引用的符号？
>
>    答：看`*`前一个token的类型。如果星号前面是表达式开始的标识符，那么证明`*`前面并没有表达式，所以此时`*`只有解引用的功能。反之则是乘法的作用。
>
>    而表达式开始的标志，要么是token下标为0；要么是token的类型为左括号`(`。
>
> 2. 在哪里判断`*`的作用？
>
>    答：在方法`expr()`中。`eval()`函数只管按照已有的表达式结构计算，不参与符号的判断。
>
> 3. 思考：既然已经判断了`*`的类型为解引用，那我该怎么计算解引用表达式？（同负数）
>
>    答：例如`*0x1234 + 678`这个表达式。
>
>    正常的四则运算只能确认此表达式的主运算符为`+`。然后分别计算表达式1：`*`、`0x1234`和表达式2：`678`的值。
>
>    首先判断前缀是否为特殊运算符（指针的解引用和负数，以及后面可能需要的自增、取地址、按位取反等）。如果是，则进入特殊运算流程。
>
>    特殊运算流程：
>
>    - 找到第一个表达式：确认前缀运算符后，第一个表达式`0x1234`
>    - 调用解引用函数处理表达式结果：根据前缀运算符，调用相应的函数进行处理第一个表达式运算后的值，得到特殊运算的结果`deference(0x1234)`
>
>    计算解引用表达式后，再与后面的`678`相加得出结果。
>
>    同理计算表达式：`*(0x1234+0x12) + 56`
>
>    步骤依旧是：
>
>    - 找到第一个表达式：`(0x1234 + 0x12)`
>    - 然后调用解引用函数获取对应地址的值
>
> 4. 需要实现的函数或者方法。
>
>    - `int locate_first_operator(int p, int q)`：从p，q下标中获取第一个运算符的位置
>    - 前缀表达式函数策略：利用函数指针来实现

### 实现监视点

主线任务：

- 理解gdb工具中的监视点功能，补充pa监视点的结构体参数`typedef struct watchpoint {} WP;`
- 实现监视点池的管理
- 实现监视点的功能：输入待监视的表达式，实现对应的监视功能。

支线任务：

- [了解`static`关键字在定义变量时候的作用](https://akaedu.github.io/book/ch20s02.html#id2787367)
- 了解为什么会在定义监视点池的时候，使用关键字`static`，可以不使用吗，为什么？
- 尽可能将潜在的问题转换为error，降低调试难度，所做的努力记录。
- 写一个测试监视点功能的方法。
- 尝试编写一个触发段错误的程序, 然后在GDB中运行它. 你发现GDB能为你提供哪些有用的信息吗?

主线任务思考：

> 1. gdb工具中的监视点的信息（结构体要存储的信息）
>
>    - **Num**：监视点的编号（`int NO`）
>    - **Address**：被监视的内存地址（`uint32_t address`）
>    - **What**：被监视的变量或表达式（`char *expr`）
>
> 2. `WP* new_wp()`
>
>    功能描述：
>
>    可以从函数`init_wp_pool()`看出，`free_`使用的是不带头节点的链表结构。那我们后面遵循的就是针对“不带头结点的”链表操作方法。
>
>    判断`free_`链表是否为空，如果不为空，则返回一个空闲的监视点结构（第一个节点）。如果为空直接`assert(0)`.
>
>    删除`free_`链表的首节点。
>
>    在`head`链表尾部新增这个空闲的监视点。
>
> 3. `void free_wp(WP *wp)`流程
>
>    `free_`末尾新增节点`wp`。
>
>    从`head`链表中，删除节点`wp`。
>
> 4. 函数`trace_and_difftest()`的功能
>
>    - 检测所有监测点，判断是否其表达式的值发生了变化`watchpoint_check_changes()`
>
>      - 如果发生变化：
>
>        输出触发监视点的信息给用户（放在函数`watchpoint_check_changes()`中，因为遍历链表的性能开销较大）；
>
>        则设置暂停效果；
>
>        并返回到主循环`sdb_mainloop()`；
>
>      - 如果没有变化，则不做任何处理。
>
> 5. 监视点的相关功能
>
>    - 函数`void expr_watchpoint_create(char *expr)`：输入表达式`expr`，则通过``new_wp()`申请空闲监视点结构，并记录表达式。

针对思考2、3点，需要实现不带头结点链表的几个操作方法：

- 删除链表的头结点：`delete_head()`
- 链表尾部插入节点：`insert_tail()`
- 删除链表中间节点：`delete_wp()`

针对思考点5，需要实现的一些功能：

- 补充`DATA`参数：加入一个`old_value`，作为对应表达式`expr`的历史计算结果。
- 实现函数`void expr_watchpoint_create(char *expr)`：输入表达式`expr`，随后通过`new_wp()`申请空闲监视点结构，并在新申请的监视点中记录此表达式
- 更新`old_value`：
  - 在函数`expr_watchpoint_create()`调用记录输入的表达式`expr`时，将此表达式的计算结果作为`old_value`的初始值。
  - 如果发生了表达式结果`result`不等于初始值`old_value`，则将`old_value`的值替换为`result`。

`watchpoint.c`需要实现的函数接口:

- `void expr_watchpoint_create(char *e)`
  - 描述：输入表达式`expr`，若有空闲节点，并且表达式合法，则新增一个含有此表达式的监测点；
  - 逻辑：
    - 校验表达式`e`的合法性
    - 申请空闲监视点结构`wp`（通过``new_wp()`）
    - 新申请的监视点中记录此表达式
    - 将表达式的计算值赋值给`old_value`
  - 输入：字符串类型的表达式`expr`
  - `bool watchpoint_check_changes()`
  - 描述：检查是否有监视点的表达式计算值发生了变化
  - 逻辑：
    - 遍历所有监测点
    - 获取每一个监测点中表达式
    - 执行表达式，获取新的计算结果并保存到`new_value`
    - 比对监测点的旧值，若发生变化，则输出新值和旧值的对比，并将旧值替换为新值
  - 输出：返回是否监测点的表达式的计算值发生了变化

实现这两个函数后，需要新增一个`watchpoint.h`来声明这两个函数。

遇到的问题思考：

> 1. 函数`delete_wp(WP **head, WP *key_wp)`中，如果`key_wp`是头指针，为什么不能用`*head = *head->next;`来删除头结点？
>
>    答：`*head->next` 实际上是**错误的语法**，因为运算符优先级的问题。
>
>    - `->` 运算符的优先级高于 `*`，因此 `*head->next` 会被解析为 `*(head->next)`。
>    - 然而，`head` 是一个指向指针的指针（`WP**`），并不包含 `next` 字段，因此这种语法会导致编译错误。
>
> 2. 在定义监视点池的时候， `static`在此处的含义是什么? 为什么要在此处使用它?
>
>    答：设置静态全局变量，这样变量名`wp_pool`、`head`以及`free_`的可见性会被限制在本文件`watchpoint.c`中。如果其他文件也可能用`head`来表示另一个链表的头结点，用静态全局变量的做法避免了变量名冲突。
>
> 3. 尽可能将潜在的问题转换为error，降低调试难度，所做的努力记录
>
>    答：已经将所有指针索引之前，都加了判空处理。
>
> 4. 如何在`trace_and_difftest()`中，返回到`sdb_mainloop()`循环中等待用户的命令？
>
>    答：函数`trace_and_difftest()`将`nemu`的状态置为`NEMU_STOP`后，会返回其被调用的函数`execute()`中。而主函数就有处理不正常状态的判断语句
>
>    ```c
>    if (nemu_state.state != NEMU_RUNNING) break;
>    ```
>
>    而此函数一旦返回，则会回到函数`cpu_exec()`中。经过后面的流程，自然回到函数`sdb_mainloop()`中。

### 断点

主线任务：

- 利用监视点，实现断点功能。

  ```c
  w $pc == ADDR
  ```

支线任务：

- 阅读“[断点的工作原理](https://eli.thegreenplace.net/2011/01/27/how-debuggers-work-part-2-breakpoints)”
  - 完成后解锁：如果把断点设置在指令的非首字节(中间或末尾), 会发生什么? 你可以在GDB中尝试一下, 然后思考并解释其中的缘由.

其实本质就是实现`TK_EQ`的表达式计算功能。

`TODO`实现这个表达式的时候，我发现如果将`{"!=", TK_NEQ},`放在判断数字的开头，会发生不能正常判断数字的问题

```C
rule {
  const char *regex;
  int token_type;
} rules[] = {
  {"\\(", '('},
  {"\\)", ')'},
  {" +", TK_NOTYPE},    // spaces
  {"\\*", '*'},     // multi
  {"\\/", '/'},     // div
  {"\\+", '+'},         // plus
  {"\\-", '-'},     // sub
  {"==", TK_EQ},        // equal
  {"!=", TK_NEQ},       // not equal
  {"(0x|0X)[0-9a-fA-F]+", TK_HEX}, // hexadecimal
  {"\\$[0-9a-zA-Z]+", TK_REG},     // register
  {"[0-9]+", TK_NUM},   // decimal
  {"&&", TK_AND},       // AND

};
```

此时如果将其放在末尾，则能解决这个问题，这个原因我现在还不是很了解。

另一个问题是：

我该怎么提升`TK_EQ`的计算优先权。ok，已经解决。

现在面临的问题：

- 为什么断点`w $pc == 0x80000004`的时候，输入`c`继续执行，并不会触发监视点？

- 为什么`w $pc`会导致程序一直能够继续执行停不下来？

- ~~修改负数和解引用等前缀表达式的判定问题（`expr`函数）~~

> 1. 为什么断点`w $pc == 0x80000004`的时候，输入`c`继续执行，并不会触发监视点？
>
>    答：
>
>    难道程序在地址`0x80000004`没有指令要执行？
>
>    经过单步调试发现，地址`0x80000004`存在指令
>
>    ```assembly
>    0x80000004: 00 02 88 23 sb      zero, 16(t0)
>    ```
>
>    所以明明`$pc`会指向这个地址，为什么没有触发监视点呢？
>
>    猜测监视点的表达式计算有问题了，需要去`watchpoint.c`中再看看。
>
>    在检测监视点表达式的值是否发生变化的函数`watchpoint_check_changes()`里，及时打印旧值和新值的结果
>
>    ```C
>    printf("wp:old_value = %d, new_value = %d\n", current->data->old_value, result);
>    ```
>
>    随后编译运行`nemu`，单步执行，并且设置观测点`w $pc == 0x80000004`.
>
>    结果是，`$pc`在执行到程序的末尾`0x8000000c`之前，居然没有更新过`$pc`的值。
>
>    所以要看下单步执行时，`$pc`的更新问题。这个需要回到`cpu-exec.c`中的单步执行函数`exec_once()`中排查问题。
>
>    OK，`$pc`应该是实时更新的`cpu.pc`，而不是`cpu_state.halt_pc`。后面这个变量很明显是终止后的`pc`值。
>
>    在获取寄存器值的位置`nemu/src/isa/riscv32/reg.c`将所有动态的`$pc`值获取方法，由`cpu_state.halt_pc`改为`cpu.pc`后，此问题已解决。
>
> 2. 为什么`w $pc`会导致程序一直能够继续执行停不下来？
>
>    答：为什么设置这个监视点后，执行到程序结尾
>
>    ```assembly
>    0x8000000c: 00 10 00 73 ebreak
>    ```
>
>    仍然停不下来？
>
>    分析监视点设置后，会对程序的正常执行造成的影响。按照pa的指导手册来看：
>
>    >  若发生了变化, 程序就因触发了监视点而暂停下来, 你需要将`nemu_state.state`变量设置为`NEMU_STOP`来达到暂停的效果. 最后输出一句话提示用户触发了监视点, 并返回到`sdb_mainloop()`循环中等待用户的命令.
>
>    这里我们需要分析触发监视点`w $pc`后，即使`pc`指向的不是指令，依然能够执行的原因。
>
>    那么思考下，正常没有设置这个触发点，程序执行到`0x8000000c`时候，能够停下来的原因。
>
>    回顾[RTFSC](https://nju-projectn.github.io/ics-pa-gitbook/ics2023/1.3.html#%E8%BF%90%E8%A1%8C%E7%AC%AC%E4%B8%80%E4%B8%AA%E5%AE%A2%E6%88%B7%E7%A8%8B%E5%BA%8F)中，客户程序执行到`ebreak`命令后，它指示了程序的结束。下面我们看看正常执行到结束指令`ebreak`后，是怎么让程序停止继续执行的。
>
>    首先是使用单步执行命令`si`直到程序结束，看下函数的执行流程。
>
>    > 单步执行到程序正常结束的流程：
>    >
>    > 1. `nemu/src/monitor/sdb/sdb.c`函数`cmd_step()`中，调用函数`cpu_exec()`
>    >
>    > 2. 函数`cpu_exec()`(`nemu/src/cpu/cpu-exec.c`中)：
>    >
>    >    首先，如果`nemu`的状态为`NEMU_END`和`NEMU_ABORT`的时候，cpu便不能再执行指令了。但是如果不是这两个(例如中断的`NEMU_STOP`)，在单步执行指令的时候，会先将`nemu`的状态设置为`NEMU_RUNNING`。
>    >
>    >    然后调用`execute()`。
>    >
>    >    调用此函数后，`cpu_exec()`函数会再次检测`nemu`的状态，依旧与上一步的状态判断相同。
>    >
>    > 3. 函数`execute()`：
>    >
>    >    首先执行函数`exec_once(&s, cpu.pc)`，单步执行一条指令。
>    >
>    >    随后调用`trace_and_difftest()`来检查观测点。这里我们没有设置观测点，所以直接忽略这个函数。
>    >
>    > 4. 函数`exec_once()`：调用函数`isa_exec_once(s)`来执行当前指令`s`
>    >
>    > 5. 函数`isa_exec_once()`（文件`nemu/src/isa/riscv32/inst.c`）:
>    >
>    >    ```C
>    >    int isa_exec_once(Decode *s) {
>    >      s->isa.inst.val = inst_fetch(&s->snpc, 4);
>    >      return decode_exec(s);
>    >    }
>    >    ```
>    >
>    >    首先从下一条指令`snpc`指向的内存地址中，取出长度为4个字节指令的值。
>    >
>    >    随后调用`decode_exec()`解析指令.
>    >
>    >    ```c
>    >    static int decode_exec(Decode *s) {
>    >      int rd = 0;
>    >      word_t src1 = 0, src2 = 0, imm = 0;
>    >      s->dnpc = s->snpc;
>    >      // ……
>    >      INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10))); // R(10) is $a0
>    >      // ……
>    >      return 0;
>    >    }
>    >    ```
>    >
>    >    遇到指令`ebreak`，会执行`NEMUTRAP(s->pc, R(10))`（在`include/cpu/cpu.h`）
>    >
>    >    ```C
>    >    #define NEMUTRAP(thispc, code) set_nemu_state(NEMU_END, thispc, code)
>    >    ```
>    >
>    >    这样函数`isa_exec_once()`执行到含有`ebreak`指令的地址时，会将`nemu`的状态置为`NEMU_END`。
>    >
>    >    所以执行完毕后，一路返回到函数`cpu_exec()`，此时函数调用完毕`execute()`后，会再次检测`nemu`的状态。此时状态为`NEMU_END`，则会正常停止程序的执行。
>
>    那我们分析下当设置检测点`w $pc`的同时，执行程序中断指令`ebreak`会发生什么:
>
>    > 1. 函数`isa_exec_once()`执行到含有`ebreak`指令的地址时，会将`nemu`的状态置为`NEMU_END`。
>    >
>    > 2. 返回到函数`execute()`，此时调用`trace_and_difftest()`来检查观测点表达式值是否发生变化。
>    >
>    >    也就是此刻，观测点`w $pc`的条件成立，即`$pc`的值发生了变化。
>    >
>    >    此时函数`trace_and_difftest()`会将原本`ebreak`已经实现的停止运行状态`NEMU_END`，**修改为检查点成立的中断状态`NEMU_STOP`**
>    >
>    > 3. 因为修改后的`NEMU_STOP`状态不能终止程序流程，所以会一直不断执行`pc`指向地址处的指令，即使`pc`指向的地址处指令不合法。
>
>    因为执行程序终止的指令`ebreak`也会修改`$pc`的值，所以我们需要执行完毕后打印监测点的变化。
>
>    所以解决方案就是在函数`trace_and_difftest()`中，加入对异常执行状态`state != NEMU_RUUNNING`的判断。
>
>    观测点值变化打印，只有在程序执行指令后，`nemu`状态仍为正常状态(`NEMU_RUUNNING`)时，才能中断程序。其他异常情况，不做处理，因为中断的优先级低于终止的优先级。

现在想实现：如果指令触发了监测点，同时输出此指令的值。类似于单步执行时候打印`pc`所指地址的值。

有个问题好奇：

> 为什么单步执行`si`调用`exec_once()`能输出此地址的指令值;但是执行`c`命令，同样地也是执行`exec_once()`，但是这时候并不会输出每次执行指令的值？
>
> 答：因为命令`c`执行`exec_once()`的时候，会传入一个大于`MAX_INST_TO_PRINT`的值，导致`g_print_step`为`false`，
>
> ```C
> if (g_print_step) { IFDEF(CONFIG_ITRACE, puts(_this->logbuf)); }
> ```
>
> 这样就不会输出已经执行的指令信息。

知道这个原理以后，我们可以简单地在`trace_and_difftest()`中，检测触发监测点后，打印此即可。

至此，从7月23号到9月24号，用时两个月，番茄TODO的有效时长为106小时50分钟。

PA1完结散花。:cherry_blossom:
