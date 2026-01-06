# PA0

## man的SYNOPSIS

| 格式               | 含义                                                         |
| ------------------ | ------------------------------------------------------------ |
| `bold text`        | **加粗** 表示你要**原样输入**的内容，如命令名、固定参数等。例如：`ls`, `-l` |
| `italic text`      | *斜体* 表示你要替换的参数，如文件名、路径等。例如：*filename* |
| `[-abc]`           | 中括号表示**可选**参数，多个字符连写表示可以组合使用，如 `-a -b -c` 可以写成 `-abc` |
| `argument ...`     | 表示这个参数可以**重复**输入多次，例如：`file1 file2 file3`  |
| `[expression] ...` | 表示一个**可选的重复参数组合**，整个表达式都可以多次出现     |

- 在终端中，man 页面通常无法显示斜体，因此会用 **下划线** 或 **颜色** 来代替。

- SYNOPSIS 并不是具体命令，而是一个通用模板，告诉你有哪些用法格式。

> SYNOPSIS 是命令使用格式的“模板”，加粗的是你照抄的，斜体的是你自己替换的，可选项用 `[]`，互斥项用 `|`，可重复用 `...`。

## GDB调试

本次旅程，我想多做点GDB调试锻炼能力。

所以如果想调试NEMU，那就需要

```bash
make gdb
```

## 关闭开发跟踪

因为现在PA默认实验过程进行跟踪，所以需要手动停止跟踪过程。

```bash
crx@crx-PC:~/study/ics2025$ git diff
diff --git a/Makefile b/Makefile
index 832c330..b72ea78 100644
--- a/Makefile
+++ b/Makefile
@@ -6,11 +6,14 @@ STUNAME = crx
 GITFLAGS = -q --author='tracer-ics2025 <tracer@njuics.org>' --no-verify --allow-empty

 # prototype: git_commit(msg)
+define git_commit_backup
+#      -@git add $(NEMU_HOME)/.. -A --ignore-errors
+#      -@while (test -e .git/index.lock); do sleep 0.1; done
+#      -@(echo "> $(1)" && echo $(STUID) $(STUNAME) && uname -a && uptime) | git commit -F - $(GITFLAGS)
+#      -@sync
+endef
+
 define git_commit
-       -@git add $(NEMU_HOME)/.. -A --ignore-errors
-       -@while (test -e .git/index.lock); do sleep 0.1; done
-       -@(echo "> $(1)" && echo $(STUID) $(STUNAME) && uname -a && uptime) | git commit -F - $(GITFLAGS)
-       -@sync
 endef

 _default:
```

