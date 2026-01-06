# CMake心得体会

> 写写理解

CMake是构建系统生成器。是生成类似`Makefile`来管理项目的工具。

```bash
CMake ---> |CMakeList.txt| ---> |Makefile| ---> make ---> 管理项目
```

## 安装CMAKE

```bash
crx@crxVMstation:~$ cmake --version
cmake version 3.28.3

CMake suite maintained and supported by Kitware (kitware.com/cmake).
```

## 项目编译方法

### ✅构建

从源代码生成可运行的程序或库。

创建测试项目`cmake_test`，其中包含一个源文件，一个构建脚本：

```bash
cmake_test/
├── CMakeLists.txt
└──main.c
```

main.c源码：

```C
// main.c
#include <stdio.h>

int main() {
    printf("Hello, CMake!\n");
    return 0;
}
```

编写 `CMakeLists.txt`：

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(HelloCMake C)

add_executable(hello main.c)
```

然后在`cmake_test`目录下，执行以下命令进行构建：

```bash
~/package $ cmake -S . -B build
~/package $ cmake --build build
```

适用于脚本或跨平台构建。

- `-S .`：指定源码目录为当前目录（Source）
- `-B build`：指定构建目录为 `build`（Build）
- `cmake --build build`等价于`cd build && make`（如果是 Unix Makefiles）

可以指定要构建的目标、并行度等：

```bash
cmake --build build --target mylib      # 只构建 mylib 目标
cmake --build build --config Release    # 用 Release 配置构建（Windows）
cmake --build build -j12                # 并发构建
```

### ✅安装

将构建好的东西**复制到系统的统一位置**，方便使用或其他程序调用。

**常见的安装位置**：

- `/usr/local/bin`：放可执行文件
- `/usr/local/lib`：放动态/静态库
- `/usr/local/include`：放头文件

```cmake
cmake --install build
```

- 安装动作依赖你在 `CMakeLists.txt` 中用 `install()` 函数指明哪些文件要安装。
- 如果你不写 `install()` 指令，`cmake --install` 什么也不会干。

> **构建是造东西，安装是搬东西。**

- 构建：把代码变成能运行的程序
- 安装：把程序放到用户/系统能找到的地方（以便调用或执行）