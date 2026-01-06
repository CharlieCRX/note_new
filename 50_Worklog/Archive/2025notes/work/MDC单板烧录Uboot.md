MDC 单板烧录过程分为 3 个步骤：

1. 替换 MDC 单板中的某些文件（内核 + 文件系统）
2. 进入U-Boot模式，配置 MDC 板子的网络并通过 TFTP 下载从 Windows 主机上的镜像 
3. U-Boot模式下，在 MDC 板子上进行烧录流程

下面就按照两个步骤，详细介绍如何实现 MDC 单板的 uboot 更新。

## 1.替换文件

> 实现这个功能需要做的：
>
> 1. 配置 Windows 主机与 MDC 单板在同一网段下
> 2. 将 Windows 主机的文件传输到 MDC 的特定位置

### 配置统一网段

首先配置 Windows 开发主机和 MDC 单板的 IP 地址处于统一网段处，为了让两台设备能**直接互相访问、快速通信和方便调试**。

> 什么是网段？
>
> - IP 地址由 **网络号** + **主机号**组成。
> - **统一网段** = 两台设备的网络号相同，它们在同一条“局域网”里。
> - 举例：
>   - Windows 主机 IP：`192.168.1.9`，子网掩码：`255.0.0.0`
>   - MDC 单板 IP：`192.168.1.20`，子网掩码：`255.0.0.0`
>      → 两者网络号都是 `192.1.1.1` → 在同一网段。

首先添加一个新的 Windows 主机 IP，类似：

```cmd
IPv4 地址 . . . . . . . . . . . . : 10.1.2.9
子网掩码  . . . . . . . . . . . . : 255.0.0.0               
```

然后登录单板，配置 MDC 单板的 IP。

1. 连接电源和串口线，设置合理的波特率后连接至开发板
2. 开机后，输入用户名`root`进入 MDC 单板系统

类似：

![image-20250911173501166](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250911173501166.png)

只要输出了：

```bash
root@localhost:~#
```

就说明进入主界面了，可以设置 IP 了。

#### 设置IP

1. 设置 IP 地址

   ```bash
   ifconfig fm1-mac3 10.1.2.254
   ```

2. 检测是否与 Windows 网络联通

   ```bash
   ping 10.1.2.9 #根据自己 Windows 主机的 IP 来，这里是演示 IP
   ```

   如果出现

   ```bash
   PING 10.1.2.9 (10.1.2.9) 56(84) bytes of data.
   64 bytes from 10.1.2.9: icmp_seq=1 ttl=128 time=0.704 ms
   64 bytes from 10.1.2.9: icmp_seq=2 ttl=128 time=0.442 ms
   64 bytes from 10.1.2.9: icmp_seq=3 ttl=128 time=0.964 ms
   ```

   就可以说明 MDC 单板已经和 Windows 主机网络通畅了，可以进行下一步了。

### 配置密码

在 MDC 单板上，输入`passwd`后，连续输入两次 `1`，将 1 替换为新的 root 密码

```bash
root@localhost:~# passwd
```

### 上传文件到 MDC 单板

配置完毕 IP 和密码后，通过 SSH 协议登录刚才配置好的 MDC 单板上，连接方法如下：

- [<u>Xshell登录服务器的两种认证方式</u>](https://www.xshellcn.com/xsh_column/renzheng-fangshi.html)（Xshell需要绿色版）
- <u>[MobaXterm连接远程Linux服务器](https://www.cnblogs.com/gis-luq/p/3993378.html)</u>（MobaXterm<u>[下载地址](https://mobaxterm.mobatek.net/download-home-edition.html)</u>）

随后就是配置将 Windows 文件传输到 MDC 单板上：[<u>通过 XShell 或者其他 SSH 工具，将 Windows 下的文件，传输到 MDC 单板上</u>](https://blog.csdn.net/weixin_43291944/article/details/89637155)。

1. 首先是 MDC 目录：`/run/media/mmcblk0p2/boot`下文件的替换

<img src="C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250911175130077.png" alt="image-20250911175130077" style="zoom:80%;" />

2. 另一个就是 MDC 根目录下：`/`

   ![image-20250911175436736](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250911175436736.png)

## 2.更新 boot

### 配置Windows Tftp 服务

MDC 单板需要通过 Windows 提供对应的`img`镜像才能更新 boot。而MDC 单板在 boot 模式下，通过 Tftp 的形式从 Windows 获取到对应的 `img`文件。

首先找到 Windows 环境下，包含`firmware_ls1046ardb_uboot_qspiboot_1133_5a59.img`的目录，例目录为：

```bash
E:\data
```

打开tftp软件 `tftpd64.exe`，将`Current Directory`设置为包含此`img`的目录：![image-20250911180257748](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250911180257748.png)

然后点击 `Show Dir`检查是否设置成功。

这样就打开了 Windows 的 TFTP 服务，可以进行下一步了。

### U-Boot 环境里配置网络并通过 TFTP 下载镜像

要进入 U-boot模式，需要重启 MDC 单板后，按住空格进入。进入界面类似：

<img src="C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250911180519740.png" alt="image-20250911180519740" style="zoom:150%;" />



此时就进入了 Uboot 环境，首先将看门狗关闭，**防止命令执行过程中重启**！！！一定先在 boot 下执行此命令，执行完毕后，为了安全起见，请等待 30 s 左右不重启即可验证关闭看门狗成功。

```bash
setenv fl_wdt_en 0
saveenv
```

确认看门狗关闭后，就可以进行配置网络了：（依次输入）

1. 设置 MDC 单板 IP

   ```BASH
   setenv ipaddr 10.1.2.254
   ```

2. 设置 TFTP windows 主机 IP（**根据实际情况修改**）

   ```bash
   setenv serverip 10.1.2.9
   ```

3. 测试 MDC 与 windows 网络是否通顺

   ```bash
   ping 10.1.2.9
   ```

4. 通过 TFTP 下载镜像

   ```bash
   tftp 0x90000000 firmware_ls1046ardb_uboot_qspiboot_1133_5a59.img
   ```

最终下载成功后，界面类似

<img src="C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250911180947047.png" alt="image-20250911180947047" style="zoom:150%;" />

此时已经下载好镜像，下一步就是烧录流程。

### 烧录

执行如下命令：

> :warning:注意：执行命令的时候，请耐心等待执行完毕到正常输出，中途卡顿是正常现象，**不要乱按回车**！！！否则将会重复执行之前的命令直到执行完毕，中途若提前终止会导致设备故障！！！

```bash
sf probe 0:0


sf erase 0 0x1000000


sf write 0x90000000 0 0x1000000


reset
```

执行结果为：

<img src="C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20250911181333698.png" alt="image-20250911181333698" style="zoom:150%;" />

执行完毕后，再次重启摁住空格进入 Uboot 模式，执行如下命令

```bash
setenv othbootargs "cma=1024M"


saveenv


reset
```

这样两次进入uboot并进行配置后，就完成了 MDC 单板烧录流程。