# Qt6 安卓环境配置

## 系统详情

- Qt版本为：Qt6.11

![image-20260330174108898](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260330174108898.png)

官方要求的安卓开发环境为：

![image-20260330174509066](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260330174509066.png)

- JDK 21
- NDK r27c

这两个很重要。

## 安装QtAndroid模块

打开`MaintenanceTool`：

![image-20260330174311859](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260330174311859.png)

这个 Qt6.11 下的`Android`组件必须要安装。

## 使用`AndroidStudio`配置环境

因为Qt后续用到的环境：

- JDK 21
- NDK r27c

均可以使用`AndroidStudio`配置完毕，所以使用此工具。

安装完毕后，打开：

```bash
File | Settings | Languages & Frameworks | Android SDK
```

### SDK Platforms配置

![image-20260330175113905](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260330175113905.png)

### SDK  Tools

![image-20260330175319274](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260330175319274.png)

![image-20260330175336339](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260330175336339.png)

这个 NDK 版本千万别选错了。

选中 NDK 版本后，我们将`Show Package Details`勾选取消，然后选中以下部分：

![image-20260330175520298](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260330175520298.png)

这样就安装完毕了。

## JKD21

这里偷了个懒，直接使用 `AndroidStudio`自带的 JDK 21 即可：

```bash
File | Settings | Build, Execution, Deployment | Build Tools | Gradle
```

![image-20260330175857373](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260330175857373.png)

## Qt Creator配置

在

```
编辑 - Preferences - SDKs
```

将之前的：

- JDK 路径：`D:\Android\Android Studio\jbr`
- SDK 路径：`D:\develop\Android`

填入即可。![image-20260330175958091](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20260330175958091.png)

OpenSSL 随意。