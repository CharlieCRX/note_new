
## 1. 方案概述

USBL 开发板上已有 `fpga` 命令行工具可操作 FPGA 寄存器，再配合 `i2c-tools` 操作 AK5538 自身寄存器，即可用纯 Shell 脚本完成 AK5538 采样率配置和验证，无需编译任何代码。

| 操作目标 | I2C 总线 | 从地址 | 使用工具 |
|---------|---------|-------|---------|
| FPGA 寄存器（时钟/复位） | I2C-1 | 0x55 | `fpga -r` / `fpga -w` |
| AK5538 寄存器 | I2C-1 | 0x10 | `i2cget` / `i2cset` |

---

## 2. BDZ → USBL 平台差异对照

### 2.1 FPGA 寄存器

| 功能 | BDZ (原平台) | USBL (新平台) |
|------|-------------|--------------|
| ADC 时钟选择 | `0x10` (ADDR_AD_CLK_SEL) | `0x2` (bit3-0) |
| ADC 复位 | `0x01` bit1 (写0复位，自动回1) | `0x5` bit2 (0=复位, 1=解复位，需手动恢复) |

### 2.2 时钟频率

BDZ 和 USBL 的时钟枚举值（0~9）完全相同，仅实际频率名称略有差异：

| 枚举值 | BDZ 频率 | USBL 频率 |
|-------|---------|----------|
| 0 | 2 MHz | 2.048 MHz |
| 1 | 3 MHz | 3.072 MHz |
| 2 | 4 MHz | 4.096 MHz |
| 3 | 6 MHz | 6.144 MHz |
| 4 | 8 MHz | 8.192 MHz |
| 5 | 12 MHz | 12.288 MHz |
| 6 | 16 MHz | 16.384 MHz |
| 7 | 24 MHz | 24.576 MHz |
| 8 | 32 MHz | 32.768 MHz |
| 9 | 49 MHz | 49.152 MHz |

> **结论**：MCLK 枚举值不变，脚本中的 `get_mclk()` 函数无需修改。

### 2.3 复位逻辑差异

| 平台 | 寄存器 | 操作 |
|------|-------|------|
| BDZ | `0x01` | 写 `1`（bit1=0 复位 ADC，之后自动跳回 1） |
| USBL | `0x5` | 先写 `0x1B`（bit2=0 复位 AD），等待后写 `0x1F`（bit2=1 解复位） |

> **关键**：USBL 的 `0x5` 是手动控制寄存器，bit4-0 分别对应 ACM8816/FPGA PLL/AD/phy/CPU。复位 AD 时必须保持其他设备解复位（写 1），因此先写 `0x1B`（仅 bit2=0），再写回 `0x1F`（bit2=1）。

### 2.4 I2C 总线

| 平台   | I2C 总线       | AK5538 地址 | FPGA 地址 |
| ---- | ------------ | --------- | ------- |
| BDZ  | `/dev/i2c-0` | 0x10      | 0x55    |
| USBL | `/dev/i2c-1` | 0x10      | 0x55    |

---

## 3. 前置条件

### 3.1 fpga 工具

已安装到 USBL 开发板，验证可用：

```bash
fpga -h
# 预期输出:
# Usage: fpga [options]
#  -r | --read      read fpga
#  -w | --write     write fpga
```

快速验证 FPGA 通信（USBL 寄存器地址已变）：

```bash
fpga -r 0x2           # 读 AD 时钟选择寄存器，输出十六进制值如 00000006
fpga -r 0x5           # 读复位控制寄存器
```

### 3.2 i2c-tools

如果开发板尚未安装：

```bash
opkg update && opkg install i2c-tools
```

验证 AK5538 通信（USBL 使用 I2C-1）：

```bash
i2cdetect -y 1        # 扫描 I2C-1，确认 0x10 和 0x55 可见
i2cget -y 1 0x10 0x02 # 读 AK5538 Control_1 寄存器
```

预期 `i2cdetect -y 1` 输出：

```
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- --
10: 10 -- -- -- -- -- -- -- -- -- -- -- 1c -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
40: UU -- -- -- -- -- -- 47 48 -- -- -- -- -- -- --
50: -- 51 -- -- -- 55 -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- 6a -- -- -- -- --
70: -- -- -- -- -- -- -- --
```

---

## 4. 关键参考表

### 4.1 USBL FPGA 寄存器

| 寄存器名 | 地址 | 位段 | 说明 |
|---------|------|------|------|
| AD 时钟选择 | `0x2` | bit3-0 | 0:2.048M, 1:3.072M, 2:4.096M, 3:6.144M, 4:8.192M, 5:12.288M, 6:16.384M, 7:24.576M, 8:32.768M, 9:49.152M |
| 复位控制 | `0x5` | bit2 | AD 复位：0=复位，1=解复位 |

### 4.2 USBL 复位控制寄存器 (0x5) 完整位定义

```
13'h5   复位控制
  bit31-bit5  保留
  bit4        ACM8816复位，0：复位；1：解复位
  bit3        FPGA内部锁相环复位，0：复位；1：解复位
  bit2        AD复位，0：复位；1：解复位
  bit1        phy复位，0：复位；1：解复位
  bit0        CPU复位，0：复位；1：解复位
```

操作 AD 复位时，需保持其他设备解复位（bit4/3/1/0=1）：
- 复位 AD：`0x1B`（二进制 `0001_1011`，bit2=0）
- 解复位 AD：`0x1F`（二进制 `0001_1111`，bit2=1）

### 4.3 AK5538 寄存器

| 寄存器名 | 地址 | 说明 |
|---------|------|------|
| `Power_Ma1` | 0x00 | 通道组 1 电源控制 |
| `Power_Ma2` | 0x01 | 通道组 2 电源控制 |
| `Control_1` | 0x02 | 采样率配置 |

### 4.4 采样率对应表

| 采样率 (KHz) | FS 编码 (Control_1) | MCLK 枚举值 | MCLK 频率 (USBL) |
|-------------|-------------------|-----------|-----------------|
| 32 | 0x6D | 8 | 32.768 MHz |
| 48 | 0x35 | 7 | 24.576 MHz |
| 64 | 0x1D | 6 | 16.384 MHz |
| 96 | 0x1D | 7 | 24.576 MHz |
| 128 | 0x05 | 6 | 16.384 MHz |
| 192 | 0x05 | 7 | 24.576 MHz |
| 256 | 0x55 | 7 | 24.576 MHz |
| 384 | 0x45 | 7 | 24.576 MHz |
| 512 | 0x65 | 8 | 32.768 MHz |
| 768 | 0x65 | 9 | 49.152 MHz |

---

## 5. 手动逐条命令测试

先手动逐步执行，确认每一步都能成功：

```bash
# ===== 第一步：关闭 ADC 时钟 =====
fpga -w 0x2 0              # 时钟选择=0 (2.048M，先切到最低)
# 验证: fpga -r 0x2 应输出 00000000

# ===== 第二步：复位 AD =====
# 注意: 只复位 bit2 (AD)，保持其他设备解复位
fpga -w 0x5 0x1B           # bit2=0 (复位 AD)，其余 bit=1 (解复位)
sleep 0.6
fpga -w 0x5 0x1F           # bit2=1 (解复位 AD)，恢复所有设备
# 验证: fpga -r 0x5 应输出 0000001f

# ===== 第三步：配置 AK5538 采样率 (以 128K 为例) =====
i2cset -y 1 0x10 0x01 0x00     # Power_Ma2 = 0x00 (关闭)
i2cset -y 1 0x10 0x02 0x05     # Control_1 = 0x05 (FS_128K)
i2cset -y 1 0x10 0x01 0x01     # Power_Ma2 = 0x01 (开启)

# ===== 第四步：设置 MCLK (16.384MHz for 128K) =====
fpga -w 0x2 6
# 验证: fpga -r 0x2 应输出 00000006

# ===== 回读验证 =====
i2cget -y 1 0x10 0x02          # 应输出 0x05
i2cget -y 1 0x10 0x01          # 应输出 0x01
```

---

## 6. 一键测试脚本

将以下内容保存为 `/tmp/test_ak5538.sh`：

```bash
#!/bin/sh
#==============================================================================
# AK5538 独立测试脚本（USBL 平台）
#
# 平台差异（相对于 BDZ）:
#   - FPGA AD时钟寄存器: 0x10 → 0x2
#   - FPGA 复位寄存器:   0x01 → 0x5 (bit2 控制 AD 复位)
#   - I2C 总线:          /dev/i2c-0 → /dev/i2c-1
#   - AK5538 地址不变:   0x10
#
# 依赖: fpga (已安装) + i2c-tools (opkg install i2c-tools)
# 用法: ./test_ak5538.sh [采样率: 32|48|64|96|128|192|256|384|512|768]
#       不带参数则默认 128K
#==============================================================================

I2C_BUS=1
AK5538_ADDR=0x10

# ---- FPGA 寄存器 (USBL) ----
REG_AD_CLK_SEL=0x2      # AD 时钟选择 (BDZ 为 0x10)
REG_RST_CTL=0x5         # 复位控制 (BDZ 为 0x01)
RST_AD_ASSERT=0x1B      # bit2=0: 复位 AD, 其余设备解复位 (0b0001_1011)
RST_AD_DEASSERT=0x1F    # bit2=1: 解复位 AD, 所有设备解复位 (0b0001_1111)

# ---- 采样率 -> FS 编码 ----
get_fs_code() {
    case $1 in
        32)   echo "0x6d" ;;
        48)   echo "0x35" ;;
        64)   echo "0x1d" ;;
        96)   echo "0x1d" ;;
        128)  echo "0x05" ;;
        192)  echo "0x05" ;;
        256)  echo "0x55" ;;
        384)  echo "0x45" ;;
        512)  echo "0x65" ;;
        768)  echo "0x65" ;;
        *)    echo "0x05" ;;
    esac
}

# ---- 采样率 -> MCLK 枚举值 ----
get_mclk() {
    case $1 in
        32)   echo "8" ;;
        48)   echo "7" ;;
        64)   echo "6" ;;
        96)   echo "7" ;;
        128)  echo "6" ;;
        192)  echo "7" ;;
        256)  echo "7" ;;
        384)  echo "7" ;;
        512)  echo "8" ;;
        768)  echo "9" ;;
        *)    echo "6" ;;
    esac
}

# ---- MCLK 枚举值 -> 频率名称 ----
get_mclk_name() {
    case $1 in
        0)  echo "2.048MHz" ;;
        1)  echo "3.072MHz" ;;
        2)  echo "4.096MHz" ;;
        3)  echo "6.144MHz" ;;
        4)  echo "8.192MHz" ;;
        5)  echo "12.288MHz" ;;
        6)  echo "16.384MHz" ;;
        7)  echo "24.576MHz" ;;
        8)  echo "32.768MHz" ;;
        9)  echo "49.152MHz" ;;
        *)  echo "${1}(枚举值)" ;;
    esac
}

FS=${1:-128}
FS_CODE=$(get_fs_code $FS)
MCLK=$(get_mclk $FS)
MCLK_NAME=$(get_mclk_name $MCLK)

echo "========================================="
echo "  AK5538 采样率配置测试（USBL 平台）"
echo "========================================="
echo "  I2C 总线     : I2C-${I2C_BUS}"
echo "  AK5538 地址  : 0x${AK5538_ADDR}"
echo "  采样率       : ${FS}K"
echo "  FS 编码      : ${FS_CODE}"
echo "  MCLK         : ${MCLK_NAME} (枚举=${MCLK})"
echo "========================================="

# ---- 1. 关闭 ADC 时钟 ----
echo ""
echo "[1/5] 关闭 ADC 时钟 (选择最低频率 2.048M)..."
fpga -w $REG_AD_CLK_SEL 0
if [ $? -ne 0 ]; then
    echo "  [FAIL] fpga -w ${REG_AD_CLK_SEL} 0"
    exit 1
fi
echo -n "  回读 AD 时钟选择 (0x${REG_AD_CLK_SEL}) = "
fpga -r $REG_AD_CLK_SEL
sleep 0.1

# ---- 2. 复位 AD ----
echo ""
echo "[2/5] 复位 AD (写 0x${RST_AD_ASSERT}，bit2=0 复位)..."
fpga -w $REG_RST_CTL $RST_AD_ASSERT
if [ $? -ne 0 ]; then
    echo "  [FAIL] fpga -w ${REG_RST_CTL} ${RST_AD_ASSERT}"
    exit 1
fi
echo "  等待 500ms..."
sleep 0.6

# ---- 3. 解复位 AD ----
echo ""
echo "[3/5] 解复位 AD (写 0x${RST_AD_DEASSERT}，bit2=1 解复位)..."
fpga -w $REG_RST_CTL $RST_AD_DEASSERT
if [ $? -ne 0 ]; then
    echo "  [FAIL] fpga -w ${REG_RST_CTL} ${RST_AD_DEASSERT}"
    exit 1
fi
echo -n "  回读 复位控制 (0x${REG_RST_CTL}) = "
fpga -r $REG_RST_CTL
sleep 0.1

# ---- 4. 配置 AK5538 采样率 ----
echo ""
echo "[4/5] 配置 AK5538 采样率..."
echo "  写 Power_Ma2 = 0x00 (关闭)"
i2cset -y $I2C_BUS $AK5538_ADDR 0x01 0x00
if [ $? -ne 0 ]; then
    echo "  [FAIL] i2cset Power_Ma2"
    exit 1
fi

echo "  写 Control_1 = ${FS_CODE}"
i2cset -y $I2C_BUS $AK5538_ADDR 0x02 $FS_CODE
if [ $? -ne 0 ]; then
    echo "  [FAIL] i2cset Control_1"
    exit 1
fi

echo "  写 Power_Ma2 = 0x01 (开启)"
i2cset -y $I2C_BUS $AK5538_ADDR 0x01 0x01
if [ $? -ne 0 ]; then
    echo "  [FAIL] i2cset Power_Ma2"
    exit 1
fi
sleep 0.1

# ---- 5. 设置 MCLK ----
echo ""
echo "[5/5] 设置 MCLK = ${MCLK_NAME}..."
fpga -w $REG_AD_CLK_SEL $MCLK
if [ $? -ne 0 ]; then
    echo "  [FAIL] fpga -w ${REG_AD_CLK_SEL} ${MCLK}"
    exit 1
fi
echo -n "  回读 AD 时钟选择 (0x${REG_AD_CLK_SEL}) = "
fpga -r $REG_AD_CLK_SEL

# ---- 6. 回读验证 ----
echo ""
echo "========================================="
echo "  配置完成，回读验证:"
echo "========================================="

echo -n "  AK5538 Power_Ma1 (0x00)  = "
i2cget -y $I2C_BUS $AK5538_ADDR 0x00

echo -n "  AK5538 Power_Ma2 (0x01)  = "
i2cget -y $I2C_BUS $AK5538_ADDR 0x01

echo -n "  AK5538 Control_1 (0x02)  = "
i2cget -y $I2C_BUS $AK5538_ADDR 0x02

echo -n "  AD 时钟选择    (0x${REG_AD_CLK_SEL}) = "
fpga -r $REG_AD_CLK_SEL

echo -n "  复位控制       (0x${REG_RST_CTL}) = "
fpga -r $REG_RST_CTL

echo ""
echo "========================================="
echo "  测试通过（USBL 平台）"
echo "========================================="
```

### 运行方式

```bash
chmod +x /tmp/test_ak5538.sh

# 默认 128K
/tmp/test_ak5538.sh

# 指定采样率
/tmp/test_ak5538.sh 48
/tmp/test_ak5538.sh 256
```

---

## 7. 全采样率遍历测试脚本

将以下内容保存为 `/tmp/test_ak5538_all.sh`：

```bash
#!/bin/sh
#==============================================================================
# AK5538 全采样率遍历测试（USBL 平台）
# 依次测试 32K ~ 768K 所有采样率配置
#==============================================================================

SAMPLES="32 48 64 96 128 192 256 384 512 768"
PASS=0
FAIL=0

echo "========================================="
echo "  AK5538 全采样率遍历测试（USBL 平台）"
echo "========================================="

for fs in $SAMPLES; do
    echo ""
    echo "--- 测试 ${fs}K ---"
    /tmp/test_ak5538.sh $fs
    if [ $? -eq 0 ]; then
        echo "  [PASS] ${fs}K"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] ${fs}K"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "========================================="
echo "  遍历完成: 通过 ${PASS}, 失败 ${FAIL}"
echo "========================================="
```

---

## 8. 命令速查（USBL 平台）

### FPGA 寄存器操作

```bash
fpga -r <addr>            # 读
fpga -w <addr> <val>      # 写
```

### AK5538 寄存器操作（I2C-1）

```bash
i2cget -y 1 0x10 <reg>        # 读
i2cset -y 1 0x10 <reg> <val>  # 写
```

### 常用寄存器地址（USBL）

| 设备 | 寄存器 | 地址 | 说明 |
|------|-------|------|------|
| FPGA | AD 时钟选择 | 0x2 | bit3-0: 0~9 对应 2.048M~49.152M |
| FPGA | 复位控制 | 0x5 | bit2: AD 复位（0=复位, 1=解复位） |
| AK5538 | Power_Ma1 | 0x00 | 通道组 1 电源 |
| AK5538 | Power_Ma2 | 0x01 | 通道组 2 电源 |
| AK5538 | Control_1 | 0x02 | 采样率 FS 编码 |
| AK5538 | Control_2 | 0x03 | — |
| AK5538 | DSD | 0x05 | DSD 模式控制 |

---

## 9. 故障排查

### 9.1 `fpga -r` 无输出或报错

```bash
# 检查 I2C 设备
ls -la /dev/i2c-1

# 扫描 FPGA 设备
i2cdetect -y 1 | grep 55
# 预期: 0x55 位置显示 "55"
```

> **注意**：若 `fpga` 工具硬编码为 `/dev/i2c-0`，在 USBL 上（FPGA 挂在 I2C-1）会失败。需要确认 `fpga` 工具是否已适配 I2C-1。

### 9.2 `i2cset`/`i2cget` 报 "Device or resource busy"

可能有其他进程持有 I2C-1 总线。先停掉相关程序：

```bash
killall collecter       # BDZ 主程序
# 或 USBL 对应主程序
```

### 9.3 AK5538 寄存器读失败（I2C-1 0x10）

```bash
# 确认 AK5538 在 I2C-1 上可见
i2cdetect -y 1 | grep "10 "
# 预期: "10: 10" — 0x10 位置显示 "10"

# 如果显示 "--"，检查：
#   1. AK5538 是否已上电
#   2. AD 是否已解复位（fpga -r 0x5，bit2 应为 1）
```

### 9.4 配置后采样率不对

确认操作顺序：

1. 关闭 AD 时钟：`fpga -w 0x2 0`
2. 复位 AD：`fpga -w 0x5 0x1B`
3. **等待 ≥500ms**
4. 解复位 AD：`fpga -w 0x5 0x1F`
5. 写 AK5538：关 Power_Ma2 → 写 Control_1 → 开 Power_Ma2
6. 设置 MCLK：`fpga -w 0x2 <枚举值>`

### 9.5 AD 复位不生效

确认写入 0x5 的值正确：

```bash
# 查看当前值
fpga -r 0x5

# 复位 AD 时应写 0x1B（其他设备保持解复位）
fpga -w 0x5 0x1B

# 解复位 AD 时应写 0x1F（所有设备解复位）
fpga -w 0x5 0x1F
```

> **常见错误**：直接写 `fpga -w 0x5 0` 会复位所有设备（CPU/phy/PLL/AD/ACM8816），导致系统异常。

---

## 10. 依赖总结（USBL）

| 工具 | 用途 | 安装方式 |
|------|------|---------|
| `fpga` | 操作 FPGA 寄存器（I2C-1, 0x55） | 已预装到开发板 |
| `i2cset` / `i2cget` | 操作 AK5538 寄存器（I2C-1, 0x10） | `opkg install i2c-tools` |
| `i2cdetect` | 扫描 I2C 总线设备 | `opkg install i2c-tools` |

> **不需要交叉编译环境，不需要 C 代码。**

---

## 11. 附录：BDZ vs USBL 完整差异对照

| 项目 | BDZ | USBL |
|------|-----|------|
| AK5538 I2C 地址 | 0x10 | 0x10（不变） |
| FPGA I2C 地址 | 0x55 | 0x55（不变） |
| I2C 总线 | `/dev/i2c-0` | `/dev/i2c-1` |
| AD 时钟选择寄存器 | `0x10` | `0x2` |
| 复位控制寄存器 | `0x01` | `0x5` |
| AD 复位位 | `0x01` bit1 | `0x5` bit2 |
| 复位方式 | 写 0x01（bit1=0），自动跳回 1 | 先写 0x1B（bit2=0），再写 0x1F（bit2=1） |
| 时钟枚举值 | 0~9（不变） | 0~9（不变） |
| AK5538 FS 编码 | 不变 | 不变 |