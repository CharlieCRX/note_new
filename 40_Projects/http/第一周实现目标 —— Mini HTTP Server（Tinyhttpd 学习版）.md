# 第一周实现目标 —— Mini HTTP Server（Tinyhttpd 学习版）

配合教材户根勤-《网络是怎样连接的》

## 第一周核心目标

第一周只有一个真正的目标：

> 理解 HTTP 请求是如何从浏览器进入程序，并最终返回响应的。

重点不是：

- 高性能
- epoll
- reactor
- 完整 HTTP 协议
- TCP 细节

而是：

# 建立完整的数据流认知

```text
Browser
    ↓
TCP Socket
    ↓
recv()
    ↓
Byte Buffer
    ↓
HTTP Parser
    ↓
HttpRequest Object
    ↓
Business Logic
    ↓
HttpResponse
    ↓
send()
    ↓
Browser
```

------

# 第一周最终效果

运行：

```bash
./mini_http_server
```

浏览器访问：

```text
http://127.0.0.1:8080
```

返回：

```html
<h1>Hello Tiny Server</h1>
```

------

# 第一周工程目标

## 1. 建立基础网络服务

掌握：

```cpp
socket()
bind()
listen()
accept()
recv()
send()
close()
```

理解：

- socket 是什么
- 浏览器如何建立连接
- 一个 HTTP 请求如何进入程序

------

## 2. 理解 HTTP 请求格式

能够观察并理解：

```http
GET / HTTP/1.1
Host: localhost:8080
Connection: keep-alive
```

重点理解：

- Request Line
- Header
- `\r\n`
- `\r\n\r\n`
- Content-Length

------

## 3. 建立 Parser 思维

实现：

```cpp
class HttpRequestParser
{
};
```

理解：

> Parser 本质是“状态机”。

能够解析：

```http
GET /index.html HTTP/1.1
```

得到：

```cpp
method == GET
path == /index.html
version == HTTP/1.1
```

------

## 4. 建立 Buffer 思维

开始避免：

```cpp
char buffer[4096];
```

开始思考：

```cpp
class ByteBuffer
{
};
```

理解：

- TCP 是字节流
- 数据可能不完整
- recv() 不等于一个完整请求

------

## 5. 建立 TDD 网络开发方式

开始编写：

```cpp
TEST(HttpRequestParser, ParseGetRequest)
```

理解：

> 网络协议天然适合 TDD。

因为：

```text
输入字节流
    ↓
Parser
    ↓
结构化对象
```

属于典型：

# Input → Output 测试模型

------

# 第一周推荐工程结构

```text
mini_net/
├── CMakeLists.txt
├── src/
│   ├── socket/
│   │   ├── TcpServer.h
│   │   └── TcpServer.cpp
│   │
│   ├── http/
│   │   ├── HttpRequest.h
│   │   ├── HttpRequestParser.h
│   │   ├── HttpResponse.h
│   │   └── HttpResponse.cpp
│   │
│   ├── buffer/
│   │   ├── ByteBuffer.h
│   │   └── ByteBuffer.cpp
│   │
│   └── main.cpp
│
├── tests/
│   ├── HttpRequestParserTest.cpp
│   └── ByteBufferTest.cpp
│
└── third_party/
```

------

# 第一周每日任务

------

# Day1 —— 最小 Socket Server

## 实现

```cpp
socket()
bind()
listen()
accept()
```

## 学习重点

观察：

- 浏览器是否自动连接
- curl 与浏览器区别
- 一个页面会建立几个连接

------

# Day2 —— recv() 读取 HTTP 请求

## 实现

打印：

```cpp
recv()
```

收到的数据。

## 学习重点

理解：

```http
GET / HTTP/1.1
Host: localhost
```

其实只是文本协议。

------

# Day3 —— Request Line Parser

## 实现

```cpp
class HttpRequestParser
```

解析：

```http
GET /index.html HTTP/1.1
```

## 学习重点

理解：

- Parser
- Token
- 状态转换

------

# Day4 —— Header Parser

## 实现

解析：

```http
Host: localhost
Connection: keep-alive
```

## 学习重点

理解：

- Key-Value 协议
- Header 设计思想
- Host 为什么重要

------

# Day5 —— Response 返回

## 实现

```http
HTTP/1.1 200 OK
Content-Length: 27

<h1>Hello Tiny Server</h1>
```

## 学习重点

理解：

- 浏览器如何判断响应结束
- 为什么必须有 Content-Length

------

# Day6 —— 重构日（极其重要）

## 不新增功能

只做：

- 拆函数
- 提取 abstraction
- Buffer 封装
- Parser 抽象
- 消除巨大函数

## 学习重点

理解：

> 网络代码本质是“数据流抽象”。

------

# Day7 —— 数据流复盘

## 任务

自己画出：

```text
Browser
↓
Socket
↓
recv
↓
ByteBuffer
↓
HttpParser
↓
HttpRequest
↓
HttpResponse
↓
send
↓
Browser
```

## 学习重点

建立：

# “网络 = 数据流 + 状态机 + Buffer”

的核心认知。

------

# 第一周必须掌握的核心概念

------

## 1. HTTP 是文本协议

你应该真正理解：

```http
GET / HTTP/1.1
```

只是字符串。

------

## 2. TCP 是字节流

不是消息。

不是包。

而是：

# 字节流

这是未来理解：

- 粘包
- 半包
- Buffer
- Reactor

的根基。

------

## 3. Parser 是状态机

例如：

```text
Parse Method
    ↓
Parse Path
    ↓
Parse Version
    ↓
Parse Header
```

这和：

- Modbus
- PLC 协议
- FPGA Packet

本质一致。

------

## 4. 网络编程本质是 Buffer 管理

未来你会逐渐理解：

```text
高性能网络编程
≈
Buffer 管理
```

------

# 第一周推荐阅读

------

## 书籍

### 《网络是怎样连接的》

重点章节：

- 浏览器生成请求
- DNS
- HTTP 请求
- TCP/IP 基础

重点不是记忆。

而是：

# 理解数据流。

------

# 第一周推荐工具

------

## 抓包工具

### [Wireshark](https://www.wireshark.org/?utm_source=chatgpt.com)

观察：

```http
GET / HTTP/1.1
```

真正如何发送。

------

## 命令行测试

### curl

例如：

```bash
curl http://127.0.0.1:8080
```

观察：

- Header
- Connection
- 返回内容

------

# 第一周结束后的能力目标

完成后，你应该能够：

------

## 1.

知道：

```text
浏览器到底发送了什么
```

------

## 2.

理解：

```text
HTTP 本质只是文本协议
```

------

## 3.

理解：

```text
TCP 是字节流
```

------

## 4.

能够：

```text
独立编写一个最小 HTTP Server
```

------

## 5.

开始建立：

# “网络架构师式的数据流思维”

这是整个学习路线最重要的基础。