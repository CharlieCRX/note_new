**核心概念**：Sysroot 是交叉编译时模拟目标板根文件系统的目录。

可以理解为：

```
宿主机 /usr/lib     -> x86 库，不能用于 ARM
sysroot/usr/lib    -> ARM 目标库，交叉编译时应该使用它
```

常见路径：

```
/path/to/toolchain/aarch64-unknown-linux-gnu/sysroot
```

笔记要点：

- 交叉编译时，编译器和链接器应该只使用 sysroot 里的头文件和库。
- Sysroot 代表目标板环境，不代表宿主机环境。
- 缺什么目标库，就应该补进 sysroot，而不是链接宿主机 `/usr/lib`。
- 编译时看到的库版本最好不要高于真实目标板运行时版本，否则生成的程序可能在目标板上报 `version not found`。