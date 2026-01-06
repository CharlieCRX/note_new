# Qt6 Android环境配置

首先明确自己安卓设备的 CPU 架构：

```bash
G:\BaiduNetdiskDownload\android-sdk-windows\platform-tools>adb shell getprop ro.product.cpu.abi
arm64-v8a
```

所以我们在选择Qt的交叉编译工具的时候，需要选择`arm64-v8a`这个 ABI 。这个交叉编译器用于编译 ARM 64位的绝大多数新手机的。

> 当你选择 android arm64-v8a 时，Qt 会调用 clang 交叉编译器，生成符合 arm64-v8a ABI 规范的二进制文件（.so / .apk 内的库），这样你的 ARM64 Android 13 手机上就能直接运行。

## SDKs配置

依次点开上方工具栏中的

> `工具` → `外部` → `配置`

![image-20250808090650842](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250808090650842.png)

进入`配置`的主界面后，点击`SDKs`配置安卓环境。

![image-20250808090422749](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250808090422749.png)

按照步骤配置完毕后，展示:

![image-20250808112557181](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250808112557181.png)

## 模拟器运行失败

但是我的安卓模拟器一直运行不起来。后来发现，是因为我电脑的 **HD630** 显卡对 Vulkan 或 OpenGL 支持不够理想，导致模拟器硬件加速启动失败。

这里如果你的电脑没有独立显卡，可以尝试使用以下步骤来排查问题：

明白！我帮你从零开始一步步来，**从创建最简单的 AVD 测试环境开始排查**，确保模拟器能正常启动，再逐步调整配置。

------

### 第一步：创建一个最简版 AVD（无 Google API，x86_64，API 33）

1. 打开命令行（CMD 或 PowerShell），确保你 `sdkmanager` 和 `avdmanager` 在 PATH 里，或者切换到 SDK 里 `cmdline-tools/latest/bin` 目录。
2. 先确认你装了简化镜像：

```bash
sdkmanager "system-images;android-33;default;x86_64"
```

1. 创建一个叫 `simple_avd` 的测试模拟器：

```bash
avdmanager create avd -n simple_avd -k "system-images;android-33;default;x86_64" --device "pixel_5" --force
```

解释：

- `simple_avd` 是名字
- 镜像是 Android 33 默认 x86_64 版本（无 Google API）
- 设备配置用 Pixel 5，常用且兼容好
- `--force` 如果有同名自动覆盖

------

### 第二步：启动模拟器（软硬件渲染都试）

1. 用软件渲染启动（绕过显卡驱动问题）：

```bash
emulator -avd simple_avd -gpu swiftshader_indirect -verbose
```

1. 如果能启动且界面正常，说明你的模拟器环境本身OK，GPU 驱动兼容是问题根源。
2. 用硬件渲染试试：

```bash
emulator -avd simple_avd -gpu auto -verbose
```

1. 看是否界面正常启动，有无报错。

------

### 第三步：若软渲染启动成功，更新显卡驱动

- 打开设备管理器，找到显卡，更新到最新驱动
- 重新尝试硬件渲染启动模拟器

------

### 第四步：如果模拟器启动失败或卡死，继续执行之前给你的【模拟器无界面卡死排查步骤】中的其他方案

------

### 额外提示

- 每次启动都带 `-verbose` 参数方便查看日志
- 如果模拟器运行起来后，卡死或黑屏，先别急着关闭，打开任务管理器看 `qemu-system-x86_64.exe` 是否还在运行
- 删除模拟器快照文件夹可以避免快照损坏导致的卡死

------

如果后面发现确实无法正常使用硬件渲染，可以直接使用以下启动命令来关闭 Vulkan 支持，强制使用 OpenGL

```bash
emulator -avd simple_avd -gpu angle_indirect
```

![image-20250808134350211](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250808134350211.png)

回到Qt中，如何实现启动模拟器成功呢？就是配置好`AVD参数`：

```bash
 -gpu angle_indirect -netdelay none -netspeed full -verbose
```

加入这段话后，就可以在Qt Creator中正常运行安卓模拟器了。

## Gradle缺失

现在运行的时候：出现了截图中的问题：

![image-20250808112248601](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250808112248601.png)

报错:

```bash
Downloading https://services.gradle.org/distributions/gradle-8.12-bin.zip

Exception in thread "main" java.io.IOException: Downloading from https://services.gradle.org/distributions/gradle-8.12-bin.zip failed: timeout (10000ms)
	at org.gradle.wrapper.Install.forceFetch(SourceFile:4)
	at org.gradle.wrapper.Install$1.call(SourceFile:8)
	at org.gradle.wrapper.GradleWrapperMain.main(SourceFile:67)
Caused by: java.net.SocketTimeoutException: Connect timed out
	at java.base/sun.nio.ch.NioSocketImpl.timedFinishConnect(NioSocketImpl.java:551)
	at java.base/sun.nio.ch.NioSocketImpl.connect(NioSocketImpl.java:602)
	at java.base/java.net.SocksSocketImpl.connect(SocksSocketImpl.java:327)
	at java.base/java.net.Socket.connect(Socket.java:633)
	at java.base/sun.security.ssl.SSLSocketImpl.connect(SSLSocketImpl.java:304)
	at java.base/sun.net.NetworkClient.doConnect(NetworkClient.java:178)
	at java.base/sun.net.www.http.HttpClient.openServer(HttpClient.java:533)
	at java.base/sun.net.www.http.HttpClient.openServer(HttpClient.java:638)
	at java.base/sun.net.www.protocol.https.HttpsClient.<init>(HttpsClient.java:266)
	at java.base/sun.net.www.protocol.https.HttpsClient.New(HttpsClient.java:380)
	at 
	...
	at org.gradle.wrapper.Install.forceFetch(SourceFile:2)
	... 2 more
Building the android package failed!
  -- For more information, run this command with --verbose.
11:11:15: The command "D:\develop\Qt6\6.9.1\mingw_64\bin\androiddeployqt.exe --input D:/develop/Qt6/Examples/Qt-6.9.1/demos/calqlatr/build/Qt_6_9_1_Clang_arm64_v8a-Debug/android-calqlatrexample-deployment-settings.json --output D:/develop/Qt6/Examples/Qt-6.9.1/demos/calqlatr/build/Qt_6_9_1_Clang_arm64_v8a-Debug/android-build-calqlatrexample --android-platform android-35 --jdk D:/develop/jdk-17.0.16.8-hotspot --gradle --gdbserver" terminated with exit code 14.
11:11:15: Error while building/deploying project calqlatr (kit: 安卓 Qt 6.9.1 Clang arm64-v8a)
11:11:15: When executing step "构建安卓 APK"
```

> Gradle 构建工具就像一个**智能的自动化工厂经理**。你告诉它你的原材料（源代码、依赖库）和最终产品（APK）的要求，它就会自动调度所有必要的“工人”（编译器、打包工具、测试工具等），按照最有效率的方式，一步步地把原材料加工成你想要的成品。

下面就搜索下，如何[解决Qt6下载gradle失败的问题](https://www.cnblogs.com/diysoul/p/18716660)。

> 在国内下载 gradle 会失败, 此时我们直接修改 gradle 的路径为本地路径. 
>
> 注意, 修改前先备份文件. 打开 Qt 安装目录下的 
>
> ```bash
> D:\develop\Qt6\6.9.1\android_arm64_v8a\src\3rdparty\gradle\gradle\wrapper\gradle-wrapper.properties
> ```
>
>  文件,将
>
> ```bash
> distributionUrl=https\://services.gradle.org/distributions/gradle-8.12-bin.zip
> ```
>
>  改为 
>
> ```bash
> distributionUrl=gradle-8.12-bin.zip
> ```
>
>  不要改版本号, 安装之后, 是哪个版本就用哪个版本, 将 url 去掉即可。
>
> `gradle.xxx.zip` 可以从 ```https://mirrors.cloud.tencent.com/gradle/``` 中下载对应的版本
>
> 在程序编译后, 将其放到编译目录下面的 `android-build\gradle\wrapper` 文件夹中。也可以直接放到 
>
> ```bash
> D:\develop\Qt6\6.9.1\android_arm64_v8a\src\3rdparty\gradle\gradle\wrapper
> ```
>
>  目录下，在编译时会自动将该目录下所有文件拷贝到程序编译文件夹对应的子目录中，这样就可以避免每次都手动拷贝文件。

## Gradle打包缓慢

安装完毕Gradle后，继续打包，发现卡死在这里。

出现 `Starting a Gradle Daemon, 10 busy Daemons could not be reused`，可能是 Gradle 在等待资源或者网络（比如首次构建要下载依赖）。

可以单独执行

```bash'
cd D:/develop/Qt6/Examples/Qt-6.9.1/demos/calqlatr/build/Qt_6_9_1_Clang_arm64_v8a-Debug/android-build-calqlatrexample
gradlew.bat assembleDebug

```

看看是否正常结束，如果卡在这里，那就是 Gradle 环境问题。

此时执行后发现

```bash
D:\develop\Qt6\Examples\Qt-6.9.1\demos\calqlatr\build\Qt_6_9_1_Clang_arm64_v8a-Debug\android-build-calqlatrexample>gradlew.bat assembleDebug
Starting a Gradle Daemon, 12 busy Daemons could not be reused, use --status for details
<-------------> 0% CONFIGURING [25s]
> root project > Resolve files of configuration 'classpath' > bcprov-jdk18on-1.77.jar > 384 KiB/7.9 MiB downloaded
```

可以发现卡就卡在下载很慢。这时候就需要墙墙全局代理了。全局代理后，再次执行后，速度就很快了。并且能够正常打包了。并且运行也正常了。![image-20250808153420208](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250808153420208.png)

以此记录下Qt6 Android的运行坑。