# Linux find 命令笔记

## 1. 为什么学习 find
- 在大型代码库中查文件
- 查找匹配的文件并自动执行命令
- Linux 下最强的递归搜索工具

---

## 2. 常用场景（按实际需求分类）
### 2.1 查找文件
```bash
find . -name "*.cpp"
find /var/log -type f
```

### 2.2 查找目录

```bash
find . -type d -name "build"
```

### 2.3 按大小查

```bash
find . -size +100M
```

### 2.4 查找后删除

```bash
find . -name "*.o" -delete
```

### 2.5 查找后执行命令

```bash
find . -name "*.log" -exec rm -f {} \;
find . -type f -exec ls -lh {} \;
```

------

## 3. 常用命令模板（套路）

```bash
find <路径> <条件> <动作>
```

常见条件：

- `-name`
- `-type f|d`
- `-size`
- `-mtime` / `-ctime`
- `-maxdepth`
- `-mindepth`

常见动作：

- `-print`
- `-delete`
- `-exec cmd {} \;`

------

## 4. 关键选项解释（只记最关键的 20%）

### 4.1 类型匹配

- `-type f` → 文件
- `-type d` → 目录

### 4.2 深度控制

- `-maxdepth 1` → 只看当前目录
- `-mindepth 2` → 跳过前两层

### 4.3 时间

- `-mtime -1` → 24 小时内修改
- `-ctime +7` → 7 天前创建

### 4.4 执行动作

- `-delete`（注意：**永远先加 `-print` 看看结果**）
- `-exec <cmd> {} \;`

------

## 5. 实战案例（用到什么，持续追加）

### 案例 1：查找所有 C 文件

```bash
find . -name "*.c"
```

### 案例 2：查找近期改动的大文件

```bash
find . -size +5M -mtime -2
```

### 案例 3：删除所有 build 缓存

```bash
find . -type d -name "build" -exec rm -rf {} +
```

------

## 6. 遇到的问题（Troubleshooting）

- `-exec` 和 `{} +` 与 `{} \;` 的区别
- 通配符被 shell 吞掉，需要加引号 `"*.log"`

------

## 7. 参考

- man find
- https://explainshell.com/