# ICS2025 PA 学习笔记

来源: [南京大学 计算机科学与技术系 计算机系统基础 课程实验 2025](https://nju-projectn.github.io/ics-pa-gitbook/ics2025/index.html)

## 笔记结构

```bash
ICS2025/
├── README.md                # 学习路线 & 索引（非常重要）
│
├── Chapters/                # ⬅ 保留教材映射
│   └── Ch01_Chapter1/
│       ├── Exercises.md
│       └── Notes.md         # 只放“章节概览 + 指向 Topic”
│
├── Topics/                  # ⭐ 真正的知识主干
│   ├── Monitor/
│   │   ├── Overview.md
│   │   └── MemoryScan.md
│   │
│   ├── Expr/
│   │   ├── 00-Overview.md
│   │   ├── 01-Lexical.md    # 你现在这块
│   │   ├── 02-Parser.md
│   │   ├── 03-Eval.md
│   │   └── Test.md
│   │
│   └── Memory/
│       ├── Overview.md
│       └── Addressing.md
│
├── Code/
│   └── expr/
│       └── mock_expr.c
│
└── References/
    ├── regex.md
    └── nemu-memory.md

```

## 写笔记时该怎么选“放哪儿”

| 内容类型         | 放哪里     |
| ---------------- | ---------- |
| 我今天学了第几章 | Chapters   |
| 机制理解         | Topics     |
| 工程实现         | Topics     |
| 实验代码         | Code       |
| 零散资料         | References |

## 目标

- PA 的笔记不是“代码复刻”，而是“认知结构 + 决策理由 + 可复用模式”

