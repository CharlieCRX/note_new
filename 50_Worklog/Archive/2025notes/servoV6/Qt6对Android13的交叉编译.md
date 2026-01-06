# Qt6交叉编译Android程序流程

## 构建套件的报错

安装完毕Qt6的Creator后，点开`构建套件`后，会发现报错：

![image-20250808091746864](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250808091746864.png)

> 错误表示，Qt Creator 已经识别出你需要为 **arm-linux-android-elf-64bit** 这个 **ABI（Application Binary Interface）** 编译代码，但它找不到任何能够为这个架构生成代码的 **编译器**。

那么这个报错的原理是什么呢？下面我们逐一分析。

这个错误 "NocompilercanproducecodeforthisOtversion,Pleasedefineoneormorecompilersfor:arm−linux−android−elf−64bit" 发生在 **Qt 编译**阶段，具体来说是 Qt Creator **配置编译器**时缺失了相应的工具链。

这个错误表示，Qt Creator 已经识别出你需要为 **arm-linux-android-elf-64bit** 这个 **ABI（Application Binary Interface）** 编译代码，但它找不到任何能够为这个架构生成代码的 **编译器**。

下面我将为您详细梳理从 Qt 源码到 Android 设备可执行文件的整个流程，以及每个步骤的作用：

------

### 1. Qt 编译（Build）

这是整个过程的第一步，也是你遇到问题的阶段。

- **流程**：Qt Creator 使用 **CMake** 来读取 `MakeLists.txt` 项目文件，生成构建脚本。然后，它调用 **编译器（Compiler）** 和 **链接器（Linker）** 将你的 C++ 源码编译成特定平台和架构的机器码。
- **用途**：将你编写的、与平台无关的 C++ 代码，转换成特定操作系统（如 Android）和 CPU 架构（如 arm64-v8a）能够理解和执行的二进制文件。

**你的错误就发生在这里**。Qt Creator 知道你要为 **arm-linux-android-elf-64bit** (即 **arm64-v8a** 架构) 编译，但它没有配置好对应的 **Android NDK (Native Development Kit)**。

**Android NDK** 它包含了针对不同 CPU 架构（如 `arm64-v8a`、`armeabi-v7a`、`x86`）的 **C/C++ 编译器**（通常是 **Clang**）。这些编译器可以将您的 C/C++ 代码编译成特定架构的机器码，生成 `.so` 动态链接库。

所以如何没有配置好对应的**Android NDK **，就会使Qt Creator 找不到 `clang` 或 `g++` 这样的编译器来执行编译任务。

------

### 2. ABI (Application Binary Interface)

ABI 是一个定义了应用程序如何与操作系统和硬件交互的规范。

- **流程**：ABI 的选择在编译前就已确定，它告诉编译器应该为哪种 CPU 架构生成代码。例如，`arm-linux-android-elf-64bit` 就是针对 **64位 ARM 架构**的 Android 设备的 ABI。
- **用途**：确保编译出的代码能够正确地在目标设备上运行。它规定了：
  - **指令集**：CPU 能够理解的机器码类型。
  - **数据类型大小和对齐方式**：例如，一个 `int` 在 32 位和 64 位系统上大小可能不同。
  - **函数调用约定**：函数参数如何传递，返回值如何返回。

如果编译的 ABI 和设备的 ABI 不匹配，应用将无法运行。

------

### 3. 设备运行 (Run on Device)

这是编译和打包完成后，将应用部署到设备上的最后一步。

- **流程**：
  1. **打包**：Qt 和 Android SDK 将编译好的二进制文件、Qt 库、资源文件等打包成一个 **`.apk`** 文件。
  2. **安装**：通过 `adb` (Android Debug Bridge) 工具将 `.apk` 文件安装到 Android 设备上。
  3. **运行**：设备上的 Android 系统加载 `.apk`，启动你的应用。
- **用途**：将你的应用交付给用户，并在用户的设备上实际运行起来。

**总结**

你遇到的错误 "Nocompiler..." 是 **Qt 编译**环节的 **配置问题**。Qt Creator 无法找到为特定 **ABI** (arm-linux-android-elf-64bit) 生成代码的编译器。

要解决它，你需要在 Qt Creator 的 **Kits** 和 **Devices** 设置中，正确配置你的 **Android NDK** 和 **Android SDK** 路径，以便 Qt Creator 能够找到正确的编译器工具链。

## SDK 和 NDK

 **Android NDK** 和 **Android SDK** 在 Android 编译流程中的具体作用。

------

### Android SDK (Software Development Kit)

可以把 Android SDK 理解为一个**工具包**，它包含了开发 Android 应用程序所需的几乎所有基础组件。

**主要作用：**

- **Java/Kotlin 代码编译和打包**：SDK 包含了用于编译 Java 和 Kotlin 代码的工具链，例如 `javac` 编译器。
- **Android API 库**：提供了各种 Android 平台的 API，让您的应用能够调用系统的功能，比如访问摄像头、网络、文件系统等。
- **构建工具**：包含了 `build-tools`，用于将编译好的代码、资源文件、清单文件（`AndroidManifest.xml`）等打包成最终的 **.apk** 文件。
- **模拟器和调试工具**：提供了 Android 模拟器（Emulator）和 `adb`（Android Debug Bridge），用于在电脑上测试和调试应用。

------

### Android NDK (Native Development Kit)

与 SDK 不同，NDK 是一个**附加工具集**，专为需要用 **C/C++** 等原生代码开发 Android 应用的场景而生。

**主要作用：**

- **C/C++ 编译器和工具链**：这是 NDK 最核心的作用。它包含了针对不同 CPU 架构（如 `arm64-v8a`、`armeabi-v7a`、`x86`）的 **C/C++ 编译器**（通常是 **Clang**）。这些编译器可以将您的 C/C++ 代码编译成特定架构的机器码，生成 `.so` 动态链接库。
- **原生 API 头文件**：提供了 Android 平台的原生 API 头文件，让 C/C++ 代码能够调用 Android 系统底层功能，例如图形渲染（OpenGL ES）、音频处理等。
- **构建工具**：包含了 `ndk-build` 或与 CMake 集成，帮助您管理和编译复杂的原生代码项目。

------

### 总结与编译流程中的角色

用一个形象的比喻来概括：

- **Android SDK** 就像是一个**总工程师**，负责整个项目的**上层规划和管理**。它处理所有 Java/Kotlin 相关的事务，最后将所有组件打包成一个完整的应用。
- **Android NDK** 则是一个**专业技师**，专门负责处理**底层、高性能的 C/C++ 代码**。它只负责将 C++ 代码编译成 `.so` 文件，然后将这些文件交给 SDK 的总工程师，由总工程师将其一起打包进 `.apk`。

在 Qt for Android 的编译流程中，它们的角色是：

1. **Qt 编译**：Qt Creator 调用 **NDK** 的 **C++ 编译器**（如 Clang），将您的 Qt C++ 源码编译成一个或多个 `.so` 文件。
2. **Android 打包**：**SDK** 的构建工具将这些 `.so` 文件，连同 Qt 提供的 Java 启动代码、资源文件等，一起打包成最终的 `.apk` 安装包。

> Qt 是 C++ 框架 → 必须用 **NDK** 编译出 ARM64 的 `.so`
>
> 然后用 **SDK** 把这些 `.so` + QML + 资源文件打包成 `.apk`，再通过 **SDK 的 adb** 安装到 Android 13 ARM64 手机上运行。
>
> | 对比项            | SDK                           | NDK                            |
> | ----------------- | ----------------------------- | ------------------------------ |
> | 主要语言          | Java/Kotlin                   | C/C++                          |
> | 用途              | 打包、调试、部署、Java 层开发 | 编译原生库、性能优化、底层接口 |
> | 输出产物          | .apk / .aab                   | .so（动态库）                  |
> | 是否依赖 CPU 架构 | 不直接依赖                    | 强依赖（要选对 ABI）           |
> | Qt 中的作用       | 打包和部署 Qt 应用            | 编译 Qt 自身和你的 C++ 代码    |

## 报错原因分析

### 1.报错解读

```kotlin
No compiler can produce code for this Qt version
Please define one or more compilers for: arm-linux-android-elf-64bit
```

- 你的 **Qt for Android ARM64-v8a kit** 需要一个能编译 `arm-linux-android-elf-64bit` 的交叉编译器
- Qt Creator 没有找到这个交叉编译器（NDK 提供的 clang）
- SDK 不直接解决这个问题，**NDK 才是关键**

### 2. 原因

Qt 6.9.1 需要：

- **NDK 版本要匹配 Qt 官方测试的版本**
- 在 Qt Creator 的 **Tools → Options → Kits → Compilers** 里，正确配置 `aarch64-linux-androidXX-clang`（NDK 自带）
- SDK 要有 **Platform-tools + Build-tools** 但它本身不会提供交叉编译器

这个错误一般是：

1. **没装 NDK**，或者装的版本 Qt 不认
2. Qt Creator 的 kit 里没绑定 NDK 编译器路径
3. SDK/NDK 路径里有中文/空格（会导致找不到）

### 3. Qt 6.9.1 + Android 13 ARM64-v8a 推荐版本组合

官方 Qt 文档（6.9.x）建议：

- **Android SDK Command-line Tools**: 最新版
- **Android SDK Platform**: API Level 33（Android 13）
- **Android Build-tools**: 最新版（比如 34.x 也兼容）
- **NDK**: **r25b**（版本号 25.2.9519653，Qt6 官方验证过）
- **JDK**: Java 17（Qt6.9 开始要求 JDK 17）

### 4. 关键点

- **SDK** 负责打包、部署（版本随意新一点就行）
- **NDK** 决定能否编出 ARM64 代码（版本必须匹配 Qt 要求）
- 你这个错误就是 **缺少 NDK 或 Qt Creator kit 里没绑定 NDK 编译器**