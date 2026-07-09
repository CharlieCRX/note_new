# SSH 公钥认证失败问题分析报告

## 1. 问题现象

为了实现 PC 对多块开发板（10.1.2.151、10.1.2.152、10.1.2.154）的自动部署，希望配置 SSH 免密登录。

在 PC 上执行：

```bash
ssh-copy-id root@10.1.2.154
```

系统提示：

```text
Number of key(s) added: 1
```

说明公钥已经成功复制到开发板。

但是再次登录：

```bash
ssh root@10.1.2.154
```

仍然提示：

```text
root@10.1.2.154's password:
```

即公钥认证没有生效。

------

# 2. 排查过程

## 2.1 检查 authorized_keys

开发板执行：

```bash
ls -l /root/.ssh
cat /root/.ssh/authorized_keys
```

结果：

- `/root/.ssh` 存在
- `authorized_keys` 存在
- 公钥内容完整

说明：

> ssh-copy-id 已经正确写入公钥。

------

## 2.2 检查权限

检查：

```bash
ls -ld /root/.ssh
ls -l /root/.ssh/authorized_keys
```

结果：

```text
/root/.ssh              drwx------
authorized_keys         -rw-------
```

符合 OpenSSH 官方要求：

| 文件            | 权限 |
| --------------- | ---- |
| ~/.ssh          | 700  |
| authorized_keys | 600  |

因此：

> `.ssh` 与 `authorized_keys` 权限正常。

------

## 2.3 检查 sshd 配置

检查：

```bash
grep -E "PubkeyAuthentication|AuthorizedKeysFile|PermitRootLogin|PasswordAuthentication|StrictModes" /etc/ssh/sshd_config
```

结果：

```text
PermitRootLogin yes
StrictModes yes
AuthorizedKeysFile .ssh/authorized_keys
PasswordAuthentication yes
```

说明：

- Root 用户允许登录
- authorized_keys 路径正确
- 开启了 StrictModes 检查

------

## 2.4 检查 OpenSSH 版本

执行：

```bash
sshd -V
```

结果：

```text
OpenSSH_9.9p2
```

说明：

开发板使用的是最新 OpenSSH。

因此可以排除：

- RSA 算法兼容问题
- 老版本 OpenSSH 问题
- Dropbear 问题

------

## 2.5 客户端调试

PC 执行：

```bash
ssh -vvv root@10.1.2.154
```

发现：

```text
Offering public key ...
```

随后：

```text
receive packet: type 51
```

说明：

客户端已经发送公钥，

但是：

> 服务端拒绝了公钥。

------

## 2.6 服务端调试

开发板启动调试模式：

```bash
/usr/sbin/sshd -ddd -p 2222
```

PC：

```bash
ssh -vvv -p 2222 root@10.1.2.154
```

服务端日志出现：

```text
Authentication refused: bad ownership or modes for directory /root
```

以及：

```text
Ignored authorized keys:
bad ownership or modes for directory /root
```

这是最终定位问题的关键日志。

------

# 3. 根本原因分析

继续检查：

```bash
ls -ldn /
ls -ldn /root
ls -ldn /etc
```

得到：

```text
drwxr-xr-x   23 1000 1000 ... /
drwx------    3 1000 1000 ... /root
drwxr-xr-x   12 1000 1000 ... /etc
```

注意：

整个 RootFS 的 owner 都是：

```text
UID = 1000
GID = 1000
```

而当前登录用户：

```bash
id
```

输出：

```text
uid=0(root)
gid=0(root)
```

即：

```text
登录用户：
UID=0

/root：
UID=1000
```

OpenSSH 开启：

```text
StrictModes yes
```

时，会检查：

- 用户 Home 目录
- ~/.ssh
- authorized_keys

要求：

> 必须属于当前登录用户。

由于：

```text
/root 属于 UID1000
```

而不是：

```text
UID0(root)
```

因此 OpenSSH 直接拒绝公钥认证。

------

# 4. 根因

本次问题的根因不是：

- ssh-copy-id
- authorized_keys
- OpenSSH 配置
- RSA 算法
- SSH 客户端

真正原因为：

> **制作 RootFS 时，目录及文件的 Owner 被错误设置为 UID=1000，而不是 UID=0(root)。**

导致：

```text
StrictModes 检查失败
```

最终：

```text
Authentication refused:
bad ownership or modes for directory /root
```

------

# 5. 临时解决方案

开发阶段可修改：

```text
/etc/ssh/sshd_config
```

将：

```text
StrictModes yes
```

改为：

```text
StrictModes no
```

重启 sshd 后，

OpenSSH 将不再检查：

- Home Owner
- Home 权限

即可正常使用公钥认证。

**适用于开发板内网调试环境。**

------

# 6. 根本解决方案

应修复 RootFS 制作流程。

正常 Linux RootFS 应满足：

| 路径  | Owner     |
| ----- | --------- |
| /     | root:root |
| /root | root:root |
| /etc  | root:root |
| /bin  | root:root |
| /usr  | root:root |

即：

```text
UID=0
GID=0
```

需要检查 RootFS 打包流程，例如：

- Buildroot
- Yocto
- 厂商 SDK
- mkfs.ubifs 打包流程
- 文件复制方式（是否使用 `cp -a` 保留 Owner）

确保镜像生成时正确保留文件的 UID/GID。

------

# 7. 对项目的影响

当前由于无法使用 SSH 公钥认证，会导致：

- 自动部署脚本（SSH + SCP）需要输入密码。
- 一键更新多块开发板程序无法实现完全自动化。
- 后续 CI/CD、自动部署、批量升级等开发效率降低。

因此，建议优先修复 RootFS 文件所有权问题，或者在开发阶段临时关闭 `StrictModes`，以支持免密登录和自动部署。