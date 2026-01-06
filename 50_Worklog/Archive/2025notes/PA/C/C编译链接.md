# 未声明的函数

## 问题描述

在NEMU的main函数中，调用了函数`init_monitor()`。但是`init_monitor` 实际上在 `monitor.c` 中定义（定义 = declaration + function body），但没有头文件将其声明暴露给 `main.c`。

那么问题是：**既然没有头文件包含声明，`main.c` 为什么还能正常使用这个函数？**

`nemu-main.c`的代码：

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

## 分析

### 1. `main.c` 中有自己的声明

在你给的代码中：

```C
void init_monitor(int, char *[]);
```

这一句 **本身就是声明**。

因此，编译器看到它之后就认为：“哦，我知道这个函数是什么参数和返回值了，我可以编译对它的调用。”

这时候编译器就不会报“未声明的函数”错误。

------

### 2. 链接阶段找到定义

虽然 `main.c` 自己没有定义 `init_monitor`，但你说它在 `monitor.c` 中定义了。

**只要编译最终生成的可执行文件时，把 `monitor.c` 编译后的目标文件一起链接上去**，链接器就能把 `main.c` 里调用的 `init_monitor()` 连接到 `monitor.c` 中的那个函数定义。

例如：

```bash
gcc main.c monitor.c -o my_program
```

那么链接器看到 `main.o` 中调用了 `init_monitor`，再看到 `monitor.o` 中定义了它，就会完成链接，不报错。

------

### 3. 为什么通常我们用头文件？

`monitor.c` 没有头文件暴露 `init_monitor`。确实，在规范项目中我们一般：

- 把函数的声明放在一个头文件 `monitor.h` 中；
- `main.c` 通过 `#include "monitor.h"` 引用声明；
- 这样避免重复写声明，保持一致性（不然函数签名改了要手动同步多个地方）。

但是技术上讲，**只要你在用之前自己写个匹配的声明就可以用**。头文件只是让这事更方便和安全。