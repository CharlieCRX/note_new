# 修改网络配置文件

修改

**确认网卡名称**：

在执行操作前，先用 `ifconfig -a` 或 `ip addr` 命令确认你的网卡名称是 `eth0` 还是其他（如 `enp0s3`），然后替换命令中的对应部分

编辑网卡配置文件 `/etc/network/interfaces`：

```bash
vi /etc/network/interfaces
```

将 `eth0` 的配置修改为静态IP：

```text
auto eth0
iface eth0 inet static
    address 10.1.2.151
    netmask 255.0.0.0
    # gateway 你的网关地址
```

保存后重启或重启网络服务即可。

## 修改rcS



这样配置好重启后，发现设备有两个 IP 地址：

```text
10.1.2.151
10.1.2.156
```

如何避免出现两个IP？这里就用到了**`rcS` **。

在

```bash
/etc/init.d/rcS
```

中的文件最后，加入

```bash
ip addr flush dev eth0
ip addr add 10.1.2.151/8 dev eth0
ip link set eth0 up
```

后重启，IP地址就设置为了单 IP 10.1.2.151 了。

**rcS本质上就是 Linux 系统启动时执行的一个 Shell 脚本。**

它的作用类似于 Windows 的"开机启动项"。

Linux 内核启动完成以后，并不会直接进入用户程序，而是：

```
BootLoader(U-Boot)
        │
        ▼
Linux Kernel
        │
        ▼
init(第一个用户态进程，PID=1)
        │
        ▼
执行 rcS
        │
        ▼
启动各种服务
```

所以：

> **rcS 就是整个用户空间初始化的总入口。**

## 为什么放到 rcS 后只有 151

以前启动流程里，有某个地方给 `eth0` 配置了 `10.1.2.156`。

例如：

```
启动网络
      │
      ▼
eth0
      │
      ▼
156
```

后来你在 rcS 最后加入：

```
ifconfig eth0 10.1.2.151 netmask 255.0.0.0
```

或者

```
ip addr flush dev eth0
ip addr add 10.1.2.151/8 dev eth0
```

那么启动流程变成：

```
启动网络
      │
      ▼
156
      │
      ▼
rcS 最后执行
      │
      ▼
flush 掉所有地址
      │
      ▼
重新配置151
```

于是：

```
156
 ↓
flush
 ↓
151
```

最后系统里自然只剩：

```
10.1.2.151
```

