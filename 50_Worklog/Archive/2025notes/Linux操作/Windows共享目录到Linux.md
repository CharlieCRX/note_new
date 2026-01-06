## 一、准备工作

1. 确认 Linux 已安装支持：

   ```bash
   sudo apt-get update
   sudo apt-get install cifs-utils
   ```

2. 确认目录存在：

   ```bash
   sudo mkdir -p /mnt/winshare
   ```

## 二、创建凭据文件

1. 新建一个凭据文件：

   ```bash
   sudo vim /etc/samba/credentials
   ```

2. 内容写入( Windows 密码)：

   ```bash
   username=Administrator
   password=123456
   ```

3. 修改权限，避免别人能读到密码

   ```bash
   sudo chmod 600 /etc/samba/credentials
   ```

## 三、修改 `/etc/fstab`

在 `/etc/fstab` 最后一行加上：

```bash
 //192.168.1.9/3.1-源码  /mnt/winshare  cifs  credentials=/etc/samba/credentials,iocharset=utf8,file_mode=0777,dir_mode=0777  0  0
```

- `192.168.1.9/3.1-源码`：是 Windows 共享目录的地址
- `/mnt/winshare`：linux上的目标挂载点

## 四、测试挂载

1. 让 fstab 生效：

   ```bash
   sudo mount -a
   ```

2. 查看是否挂载成功：

   ```bash
   df -h | grep winshare
   ```

## 五、下次开机自动挂载

现在就已经是永久挂载了，重启虚拟机后 `/mnt/winshare` 会自动挂载 Windows 的共享目录。