替换的文件目录：

```bash
/home/crx/ssd2351/source/kernel/sound/soc/sstar/pcupid/



/home/crx/ssd2351/source/kernel/sound/soc/codecs/
```



```bash
#全局编译，只要运行，会把boot,kernel，project，sdk编译
$ cd ~/ssd2351/source/project/
$ make myzr-ssd2351-ek112_128m_defconfig    (ddr；128M)
或者
$ make myzr-ssd2351-ek112_256m_defconfig    (ddr；256M)
$ make clean;make image -j8

#编译完成后生成的images在project/image/output/images
#注意：
#首次编译请务必在project下执行make clean;make image -j8命令完整编译（包含整编boot/kernel）
#为增加调试效率，除首次编译外，后续debug可以直接在project下编译对应修改模块然后重新快速打包即可，例如：

#仅编译kernel：
$ cd ~/ssd2351/source/project/
$ make linux-kernel_clean;make linux-kernel -j8

#仅编译boot：
$ cd ~/ssd2351/source/project/
$ make boot_clean;make boot -j8

#仅快速打包sdk image：
$ cd ~/ssd2351/source/project/
$ make image-fast-nocheck -j8
```

- 后续直接使用

  ```bash
  make image-fast-nocheck -j8
  ```

- 编译完成后生成的images在

  ```bash
  /home/crx/ssd2351/source/project/image/output/
  ```

  