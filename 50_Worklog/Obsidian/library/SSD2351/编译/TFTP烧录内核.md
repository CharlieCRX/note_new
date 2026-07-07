打开tftp服务器，默认编译好的镜像路径

```bash
project/image/output/images
```

把 images 文件夹复制电脑，在`Current Directory`中选择这个复制的文件夹位置。

```bash
#板子需要进入启动模式：启动拨码模式：1: on, 2: on 3: off 4 : off
#插入TYPEC线，开机按住Enter不放进入到Uboot控制台，按照以下方式设置IP
setenv ipaddr 192.168.1.150; //设置板端ip，要求能跟PC端ping通

setenv serverip 192.168.1.100； //设置PC端的ip

setenv -f ethact sstar_emac; //设置使用Emac,本平台使用的是Emac

setenv -f ethaddr 00:11:22:33:44:55; //设定mac地址

setenv -f netmask 255.255.255.0; //设置掩码

setenv -f gatewayip 192.168.1.1; //设置网关

estart //初始化网络 uboot 下使用网络之前需要先输入该命令

phy_w 0 0x2100

phy_w 0 0x140  


estar
#烧写成功，会自动启动板子
#与全烧录的区别是此方式可以estar auto_update.txt中的脚本，烧录任意单独分区
```