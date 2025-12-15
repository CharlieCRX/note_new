------

# ICS2025 · 计算机系统基础（PA / NEMU 学习记录）

> 本仓库用于记录[《计算机系统基础（ICS）》课程（2025）](https://nju-projectn.github.io/ics-pa-gitbook/ics2025/index.html)的学习与实现过程。

本笔记强调：

- **概念 → 工程 → 可验证代码**
- 记录完整的思考与试错路径
- 为后续系统方向（OS / 编译器 / 架构）打基础

------

## 📂 项目结构说明

```text
ICS2025/
├── Chapter/        # 按课程章节组织的系统知识
├── Topics/         # 围绕 PA 的工程专题（博客核心）
├── Code/           # 可运行 / 可测试的代码实现
└── README.md
```

------

## 📘 Chapter：课程主线笔记

```text
Chapter/
└── Ch01_Chapter1/
    ├── Notes.md
    ├── Exercises.md
    └── References.md
```

### Chapter 的定位

- 跟随 ICS 教材与课堂内容
- 关注 **“系统在做什么 / 为什么这么设计”**
- 不追求代码细节

------

## 🔧 Topics：PA 工程专题（重点）

```text
Topics/
└── Expr/
    └── 01-Lexical.md
```

### Topics 的定位

- 以 PA / NEMU 为背景
- 聚焦 **一个具体工程问题**
- 记录：
  - 设计思路
  - 实现策略
  - 边界情况
  - 测试方法

**这是未来博客发布的主要来源。**

------

## 💻 Code：可运行实现

```text
Code/
└── expr/
    └── mock_expr.c
```

### Code 的定位

- 不写说明性文字
- 保持：
  - 可编译
  - 可测试
  - 可复用
- 是 Topics 中结论的“事实依据”

------

## 🧪 测试理念

在 PA 的实现中，尽量做到：

- 核心逻辑可单独拆分
- 不依赖 NEMU 全局状态
- 能在普通 `main()` 中验证行为

例如：

- 将 `expr.c` 拆分为 `mock_expr.c`
- 构建独立的 Token 测试框架
- 验证词法分析的正确性

------

## 📝 博客发布说明

本仓库结构可直接映射为博客文章：

```text
Topics/Expr/01-Lexical.md
→ /ics/pa/expr/lexical-analysis/
```

建议每篇文章包含：

1. 问题背景
2. 设计决策
3. 实现要点
4. 测试方法
5. 总结与反思

------

## 🎯 学习目标

- 能读懂并修改 NEMU 核心代码
- 能将抽象系统概念转化为工程实现
- 建立完整的系统思维路径

------

## 📚 References

- 《Computer Systems: A Programmer’s Perspective》
- NEMU / PA 官方文档
- RISC-V ISA Specification

------

## 四、下一步我建议你立刻做的 3 件事

**不要拖，这是“体系定型”的关键期：**

1. ✅ 给 `Topics/Expr/01-Lexical.md` 加上目录骨架（我可以直接帮你写）
2. ✅ 在文中引用：`Code/expr/mock_expr.c`
3. ✅ 在 `README.md` 中给 Expr 专题加一个链接

------

如果你愿意，下一步我可以**直接帮你把 `01-Lexical.md` 写成一篇可发布博客的完整初稿**（按你现在的测试框架来）。
