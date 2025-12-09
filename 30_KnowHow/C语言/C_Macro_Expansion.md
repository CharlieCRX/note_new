# 📘 C 预处理器：宏实参与宏展开机制总结

> 思考一个案例（来自ICS2025）：
>
> 如何理解
>
> ```C
> #define DEBUG 1
> IFDEF(DEBUG, printf("Test IFDEF"));
> ```
>
> 宏展开`nemu/include/macro.h`中的`IFDEF`，其定义如下：
>
> ```C
> #define IFDEF(macro, ...) MUXDEF(macro, __KEEP, __IGNORE)(__VA_ARGS__)
> #define MUXDEF(macro, X, Y)  MUX_MACRO_PROPERTY(__P_DEF_, macro, X, Y)
> 
> #define CHOOSE2nd(a, b, ...) b
> #define MUX_WITH_COMMA(contain_comma, a, b) CHOOSE2nd(contain_comma a, b)
> #define MUX_MACRO_PROPERTY(p, macro, a, b) MUX_WITH_COMMA(concat(p, macro), a, b)
> 
> #define concat(x, y) concat_temp(x, y)
> #define concat_temp(x, y) x ## y
> 
> #define __P_DEF_0  X,
> #define __P_DEF_1  X,
> #define __P_ONE_1  X,
> #define __P_ZERO_0 X,
> 
> #define __IGNORE(...)
> #define __KEEP(...) __VA_ARGS__
> ```
>

## 1. 宏展开的三条核心原则

### **原则 1：只有宏调用才会展开**

仅仅出现宏名不会展开：

```c
A       // 不会展开
A()     // 才会展开
A(x)    // 展开
```

------

### **原则 2：宏实参在代入形参前会被完全展开（除非被 # 或 ## 阻止）**

```c
#define X 100
F(X)    // 传入 F 的实参是 100，而不是 X
```

------

### **原则 3：宏展开的结果会被重新扫描（recursive expansion）**

```c
#define A B
#define B C
#define C 1

A → B → C → 1
```

------

## 2. 宏实参展开的完整时机

预处理器处理宏调用时遵循以下步骤：

### **Step 1：识别宏调用**

例如：

```c
IFDEF(DEBUG, printf("X"));
```

------

### **Step 2：展开所有实参（除 #、## 情况外）**

```c
#define DEBUG 1
```

因此：

```
IFDEF(1, printf("X"));
```

------

### **Step 3：把展开后的实参代入形参**

例如：

```c
#define IFDEF(m, ...) MUXDEF(m, __KEEP, __IGNORE)(__VA_ARGS__)
```

代入：

```
MUXDEF(1, __KEEP, __IGNORE)(printf("X"))
```

此时 `__KEEP` 和 `__IGNORE` **只是文本，不会展开**。

------

### **Step 4：继续递归展开（MUXDEF → MUX_WITH_COMMA → CHOOSE2nd）**

直到最终形成：

```
__KEEP(printf("X"))
```

------

### **Step 5：遇到宏调用形式后展开 __KEEP**

```c
#define __KEEP(...) __VA_ARGS__

__KEEP(printf("X")) → printf("X")
```

------

## 3. 为什么 __KEEP 与 __IGNORE 一开始不展开？

它们一开始只是作为“宏实参文本”出现：

```
MUXDEF(1, __KEEP, __IGNORE)
```

不是如下形式：

```
__KEEP(...)
__IGNORE(...)
```

预处理器 **不会展开单独出现的宏名**，只会展开：

- 宏调用表达式（MACRO(...)）
- 宏实参中的可展开部分

只有最终决策链选中：

```
__KEEP(printf("X"))
```

时才会真正展开。

------

## 4. 实参展开抑制：# 与

### **1）# 字符串化会阻止对实参的展开**

```c
#define S(x) #x
#define A 100

S(A)    // "A"（不是 "100"）
```

------

### **2）## token 拼接也会阻止实参展开**

```c
#define JOIN(a, b) a##b
#define A 100

JOIN(__VAR_, A)   // → __VAR_A   （不是 __VAR_100）
```

------

## 5. 条件宏示例：IFDEF 宏展开过程

常见的条件宏系统（节选）：

```c
#define __P_DEF_1  X,
#define __KEEP(...) __VA_ARGS__
#define __IGNORE(...)

#define concat(x, y) x##y
#define MUX_MACRO_PROPERTY(p, m, a, b) MUX_WITH_COMMA(concat(p, m), a, b)
#define CHOOSE2nd(a, b, ...) b
#define MUX_WITH_COMMA(c, a, b) CHOOSE2nd(c a, b)
#define IFDEF(m, ...) MUX_MACRO_PROPERTY(__P_DEF_, m, __KEEP, __IGNORE)(__VA_ARGS__)
```

对于：

```c
#define DEBUG 1
IFDEF(DEBUG, printf("Hello"));
```

展开步骤：

1. DEBUG → 1
2. concat(_*P_DEF*, 1) → __P_DEF_1
3. __P_DEF_1 → `X,`（带逗号）
4. CHOOSE2nd 选中 `__KEEP`
5. `__KEEP(printf("Hello"))` → `printf("Hello")`

------

## 6. 宏展开流程图（最核心）

```
识别宏调用
      ↓
宏实参完全展开（不含 # 与 ##）
      ↓
用实参替换形参
      ↓
对替换结果重新扫描
      ↓
继续展开直到无法展开
```

掌握这张流程图，就掌握了宏展开的本质。

------

## 7. 常见错误总结

- ❌ 认为“所有宏名出现都自动展开” —— 实际只有调用才展开
- ❌ 认为“实参永远不会被展开” —— 实际是先展开再代入
- ❌ 忽略 # 与 ## 对展开的抑制
- ❌ 忽略“递归展开”导致的一连串替换

## 8.案例完整解析

```C
IFDEF(DEBUG,printf("Test IFDEF");)
```

替换过程：

> 调用 IFDEF 时，DEBUG 作为实参，预处理器会先把 DEBUG 替换为 1，再传入宏。
>
> （宏实参在替换进入宏体之前必须先展开）

所以一开始就会展开为：

```C
IFDEF(DEBUG,printf("Test IFDEF");) ⟶ IFDEF(1,printf("Test IFDEF");)
```

1. 展开 `IFDEF(macro,…)`

   ```C
   IFDEF(1,printf("Test IFDEF");) ⟶ MUXDEF(1,__KEEP,__IGNORE)(printf("Test IFDEF");)
   ```

2. 展开 `MUXDEF(macro,X,Y)`

   ```C
   MUXDEF(1,__KEEP,__IGNORE)(…) ⟶ MUX_MACRO_PROPERTY(__P_DEF_, 1, __KEEP,__IGNORE)(…)
   ```

3. 展开 `MUX_MACRO_PROPERTY(p, macro, a, b)`

   ```C
   MUX_MACRO_PROPERTY(__P_DEF_, 1, __KEEP,__IGNORE)(…) ⟶ MUX_WITH_COMMA(concat(__P_DEF_, 1), __KEEP,__IGNORE))(…)
   ```

4. 展开 `concat`:

   ```C
   MUX_WITH_COMMA(__P_DEF_1, __KEEP, __IGNORE)(printf("Test IFDEF"));
   ```

5. 展开 `MUX_WITH_COMMA(contain_comma, a, b)`

   > 这里需要注意的是，`CHOOSE2nd`的`contain_comma`和`a`之间没有`，`分割

   ```C
   MUX_WITH_COMMA(__P_DEF_1, __KEEP,__IGNORE)(…) ⟶ CHOOSE2nd(__P_DEF_1 __KEEP, __IGNORE)(…)
   ```

6. 实参替换 `__P_DEF_1`为带逗号的 `token`

   ```C
   CHOOSE2nd(__P_DEF_1 __KEEP, __IGNORE)(…) ⟶ CHOOSE2nd(X, __KEEP, __IGNORE)(…)
   ```

   其中：

   ```C
   CHOOSE2nd(  X,   __KEEP   , __IGNORE )
                ↑      ↑         ↑
                a      b       __VA_ARGS__
   ```

7. 展开 `CHOOSE2nd(a, b, ...)`

   ```C
   CHOOSE2nd(X, __KEEP, __IGNORE)(…) ⟶ __KEEP(…)  ⟶ __KEEP(printf("Test IFDEF");)
   ```

8. 展开 `__KEEP`

   ```C
   __KEEP(printf("Test IFDEF"))
    → printf("Test IFDEF")
   ```



