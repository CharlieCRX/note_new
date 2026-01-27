# RK3568 无 USB Download、无 Type-C 条件下

## 使用 SD 卡复现「U 盘升级 boot.img」的完整清单

> ⚠️ 本清单**明确针对：升级 eMMC 的 boot 分区（boot.img）**

------

## 一、问题背景（WHY）

-  RK3568 开发板 **未预留 Type-C / USB OTG**
-  无法进入 **USB Download 模式**
-  官方手册提供的升级方式为：
  **U 盘 + U-Boot 下升级 `boot.img`**
-  项目要求：
  **不能从 SD 卡作为最终启动方式**
-  现状：
  -  可通过 **SD 卡进入 U-Boot**
  -  可识别 SD 卡
  -  SD 卡中已准备好 `boot.img`

🎯 **目标**

> 在 **不使用 USB、不改变最终启动介质** 的前提下，
> **用 SD 卡复现手册中的“U 盘升级 boot.img”流程**

------

## 二、核心思路（WHAT）

> **把“U 盘升级 boot.img”的流程，等价替换为“SD 卡升级 boot.img”**

| 手册原方案       | 本次等价方案  |
| ---------------- | ------------- |
| usb start        | mmc dev 1     |
| fatls usb 0:1    | fatls mmc 1:1 |
| fatload usb      | fatload mmc   |
| mmc write → eMMC | 完全一致      |

✔ 写入目标仍然是 **eMMC（mmc 0）**
✔ SD 卡 **只作为升级介质**

------

## 三、设备确认清单（必须完成）

### 1️⃣ 列出 mmc 设备

```bash
=> mmc list
```

示例：

```text
dwmmc@fe2b0000: 1
dwmmc@fe2c0000: 2
sdhci@fe310000: 0 (eMMC)
```

------

### 2️⃣ 确认 SD 卡（升级介质）

```bash
=> mmc dev 1
=> mmc info
```

已验证输出（你提供的）：

```text
Name: SD64G
SD version 3.0
Capacity: 59.5 GiB
=> fatls mmc 1:1
boot.img
```

✔ **结论：mmc 1 = SD 卡（升级源）**

------

### 3️⃣ 确认 eMMC（写入目标）

```bash
=> mmc dev 0
=> mmc info
```

✔ **结论：mmc 0 = eMMC（boot 分区所在设备）**

------

## 四、升级文件确认（非常重要）

### 升级文件

-  文件名：`boot.img`
-  来源：官方/项目提供
-  用途：**写入 eMMC 的 boot 分区**

> ⚠️ 在你们方案中：
> **boot.img ≠ U-Boot 本体**
> 而是**用于启动的 boot 分区镜像（通常是 kernel + dtb + ramdisk）**

这与手册完全一致。

------

## 五、标准升级流程（SD 等价替换 U 盘）

### 1️⃣ 从 SD 卡加载 `boot.img` 到内存

```bash
=> mmc dev 1
=> fatload mmc 1:1 0x0c000000 boot.img
```

成功标志示例：

```text
26130944 bytes read in xxxx ms
```

------

### 2️⃣ 切换到 eMMC（再次确认）

```bash
=> mmc dev 0
```

> ⚠️ 心中再次确认：
> **现在操作的是 eMMC（mmc 0）**

------

### 3️⃣ 写入 eMMC 的 boot 分区（与手册一致）

```bash
=> mmc write 0x0c000000 0x8000 0x20000
```

成功标志：

```text
131072 blocks written: OK
```

------

### 4️⃣ 重启系统

```bash
=> reset
```

------

## 六、升级后验证清单

-  拔掉 SD 卡
-  板子从 **eMMC 正常启动**
-  系统运行正常
-  新 kernel / 版本生效（如有版本号变化）

------

## 七、风险与注意事项（务必阅读）

- ⚠️ **不要改动写入偏移**

  ```text
  0x8000 + 0x20000
  ```

  → 这是你们平台 **boot 分区的固定布局**

- ⚠️ 不要把 `boot.img` 写到 SPL / U-Boot 区

- ⚠️ 写入前一定确认：

  ```bash
  => mmc dev 0
  ```

- ⚠️ SD 卡只是升级工具，不是最终启动方式

------

## 八、工程结论（可直接写进项目文档）

> 在 RK3568 平台上，即使无法使用 USB Download，
> 仍可通过 **SD 卡在 U-Boot 阶段复现官方“U 盘升级 boot.img”流程**，
> 实现 **eMMC boot 分区的在线升级**，且不改变最终启动介质。

------

## 九、可选增强（以后可做）

-  升级前后加入 `crc32` / `md5` 校验
-  在 Linux 中用 `dd` 实现同等升级
-  封装为现场升级 SOP
-  与 OTA 升级流程统一

------

如果你愿意，我可以下一步帮你做：

- ✅ **把这份清单整理成 Markdown / 内部 Wiki 版本**
- ✅ **对 `0x8000 / 0x20000` 做一次“来源解释”（方便评审）**
- ✅ **给出 Linux 下等价升级命令，方便以后不用 SD**

你只要说一句：**下一步给谁用、用在什么场景**即可。	