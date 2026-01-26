# 监视点 TDD 开发清单

## 1️⃣ 准备工作

- 创建 `watchpoint.c / watchpoint.h` 模块，提供对外接口：

  ```c
  WP* new_wp(const char* expr_str);           // 创建监视点
  void free_wp(int NO);                        // 删除监视点
  WatchpointDiffResult watchpoint_diff_and_collect(void); // 检查触发
  void info_watchpoints(void);                // 打印所有已启用监视点
  ```

- 定义 `WP` 和 `WatchpointDiffResult` 结构体

  ```C
  typedef struct watchpoint {
    int NO;                     // 编号
    char expr[128];             // 原始表达式字符串
    int64_t last_value;         // 上一次求值结果
    struct watchpoint *next;    // 链表指针
    bool enabled;               // 是否启用（可选，方便 TDD）
  } WP;
  
  
  
  typedef struct {
    bool triggered;                  // 是否至少有一个监视点触发
    int  count;                      // 触发监视点数量
    const WP *list;                  // 触发监视点只读信息数组
  } WatchpointDiffResult;
  ```

- **测试框架**（用 `assert()`）

------

## 2️⃣ 步骤一：创建监视点

**目标**：能成功创建一个监视点，并且存储输入的表达式。

### 测试用例

1. 创建一个监视点 `"a + b"`：
   - 检查返回的 WP 指针不为空
   - 检查 `wp->expr` = `"a + b"`
   - 检查 `in_use = true`
   - 检查编号 NO 唯一且连续
2. 创建多个监视点：
   - 检查链表顺序正确（最近创建的排在前或后，根据你的设计）
3. 创建超过最大数量（假设你有 MAX_WP）：
   - 检查返回 NULL 或报错

------

## 3️⃣ 步骤二：删除监视点

**目标**：能按编号删除监视点

### 测试用例

1. 删除一个存在的监视点：
   - `free_wp(NO)`
   - 检查 `in_use = false`
   - 检查链表结构仍然正确
2. 删除一个不存在或已经删除的监视点：
   - 不 crash
   - 返回适当错误码或忽略
3. 删除后重新创建：
   - 新创建的监视点可以复用空闲 slot

------

## 4️⃣ 步骤三：检查监视点触发（核心功能）

**目标**：在 CPU 执行完一条指令后，正确判断哪些监视点触发，并返回信息结构。

### 测试用例

1. 设置一个简单表达式（如 `x`），初始化 `last_value = 0`
2. 模拟 CPU 执行改变了 `x = 5`
3. 调用 `watchpoint_diff_and_collect()`：
   - 检查返回值 `triggered = true`
   - 检查 `count = 1`
   - 检查 `list[0]->NO` 和 `list[0]->expr`
4. 再次调用（没有变化）：
   - 检查返回值 `triggered = false`
   - `count = 0`
5. 多个 watchpoint 同时触发：
   - 检查返回的 list 顺序（最近创建优先）
   - 检查 count = N
6. CPU 执行完一条指令但没有任何变化：
   - `triggered = false`

> ✅ 注意：这里可以模拟 CPU 状态，不必真正执行 CPU 指令，只需要修改用于表达式求值的状态数据

------

## 5️⃣ 步骤四：打印所有已启用的监视点

**目标**：能按 gdb 风格打印监视点信息

### 测试用例

1. 创建多个监视点，调用 `info_watchpoints()`：
   - 检查输出包含 NO、expr、last_value
   - 检查顺序正确
2. 删除部分监视点后再打印：
   - 检查已经删除的监视点不打印

------

## 6️⃣ 步骤五：集成到 CPU 执行流（可选）

**目标**：在 `trace_and_difftest()` 中调用 watchpoint 检查逻辑，CPU 能够在触发时停下

### 测试用例

1. 模拟 `cpu_exec(1)` 执行一条指令：
   - watchpoint 触发 → `nemu_state.state == NEMU_STOP`
   - 输出触发信息
2. watchpoint 没触发：
   - `nemu_state.state == NEMU_RUNNING`

------

## ✅ TDD 开发顺序建议

**建议顺序（每一步红-绿-重构）**：

1. **创建监视点** → 测试创建、链表管理、表达式存储
2. **删除监视点** → 测试释放、链表维护
3. **触发检查** → 核心 diff/compare 功能
4. **打印监视点** → info w
5. **CPU 集成** → trace_and_difftest 调用

> 每个步骤都先写测试，然后实现最小功能，通过测试后再重构。
> 核心原则：**先保证每块独立正确，再集成到 CPU 执行循环中**

------

