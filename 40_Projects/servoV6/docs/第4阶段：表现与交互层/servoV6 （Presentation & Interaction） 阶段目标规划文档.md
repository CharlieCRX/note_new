------

# 📘 servoV6 Phase 6 阶段目标规划文档（Presentation & Interaction）

------

# 一、阶段定义

------

## 🎯 阶段名称

```text
Phase 6：Presentation & Interaction Layer
```

------

## 🎯 阶段目标（一句话）

```text
将“可验证的单轴控制系统”

→

升级为“可交互、可演示、可操作的控制产品”
```

------

## 📅 DDL（明确时间）

```text
🟢 MVP交付：5天（40小时）
🟡 稳定交付：7天（56小时）
```

------

# 二、阶段核心交付物（必须完成）

------

# 🥇 交付物1：AxisViewModel（核心桥梁）

------

## 🎯 目标

```text
提供 Domain + Application → UI 的唯一入口
```

------

## ✅ 必须能力

------

### 1️⃣ 状态输出（只读）

```cpp
AxisState state;
double absPos;
double relPos;
double jogVelocity;
double moveVelocity;
bool isEnabled;
bool hasError;
QString errorMessage;
```

------

### 2️⃣ 控制输入（命令入口）

```cpp
void jogPositivePressed();
void jogPositiveReleased();

void jogNegativePressed();
void jogNegativeReleased();

void moveAbsolute(double pos);
void moveRelative(double delta);

void stop();
void enable();
void disable();
```

------

### 3️⃣ 驱动机制

```cpp
void tick();   // 系统推进
```

------

------

# 🥈 交付物2：QtQuick 控制面板（UI）

------

## 🎯 最小可用界面（MVP）

------

### 必须组件

```text
✔ 位置显示（abs / rel）
✔ 当前状态（Idle / Moving / Jogging / Error）
✔ Enable状态灯
✔ 正转/反转点动按钮（按下/松开）
✔ 绝对/相对定位输入
✔ Stop按钮
✔ 错误提示区域
```

------

------

# 🥉 交付物3：系统运行 Demo

------

## 🎯 必须支持场景

------

### 场景1：点动

```text
按住 → 连续运动
松开 → 停止
```

------

### 场景2：定位

```text
输入位置 → 自动执行 → 到位
```

------

### 场景3：异常

```text
限位触发 → 停止 + 报警
```

------

------

# 三、验收指标（必须全部满足）

------

# 🧪 1. 功能正确性（Functional）

------

## 点动

```text
✔ 按下后 1~2 tick 内进入 Jogging
✔ 位置连续变化（非0变化）
✔ 松开后停止（无漂移）
```

------

## 定位

```text
✔ 自动 Enable → Move → Disable
✔ 最终位置误差 < POSITION_EPSILON
✔ 不允许“假完成”
```

------

## Stop

```text
✔ 任意时刻 Stop → 立即停止
✔ 速度归零
✔ 无继续位移
```

------

------

# 🧪 2. 状态一致性（State Consistency）

------

```text
✔ UI状态 == Axis反馈状态
✔ 不允许 UI显示与真实状态不一致
✔ 无跳变错误（Idle → Moving → Idle 正确）
```

------

------

# 🧪 3. 命令正确性（Command Integrity）

------

```text
✔ 所有操作必须通过 ViewModel
✔ 不允许 UI直接调用 Axis / UseCase
✔ pendingIntent 最终必须清空
```

------

------

# 🧪 4. 可观测性（Observability）

------

```text
✔ UI实时反映：
    state / pos / velocity / error
✔ 可看到完整行为变化过程
```

------

------

# 🧪 5. 稳定性（Stability）

------

```text
✔ 连续操作 100+ 次无崩溃
✔ 无状态卡死（Deadlock）
✔ 无“永远pending”状态
```

------

------

# 🧪 6. 回归测试（Regression）

------

```text
✔ 所有现有 tests 全部通过
✔ system_integration 无回归
```

------

------

# 四、评测标准（量化评分）

------

# 📊 总分：100分

------

## 1️⃣ 功能完整性（30分）

| 项目     | 分值 |
| -------- | ---- |
| 点动正确 | 10   |
| 定位正确 | 10   |
| Stop正确 | 10   |

------

## 2️⃣ 状态一致性（20分）

```text
UI 与 Axis 完全一致
```

------

## 3️⃣ 系统稳定性（20分）

```text
无崩溃 / 无死锁 / 无卡死
```

------

## 4️⃣ 可观测性（15分）

```text
状态透明，可追踪
```

------

## 5️⃣ 架构符合性（15分）

```text
✔ UI 不侵入 Domain
✔ ViewModel 单一入口
✔ 无跨层调用
```

------

------

# 五、执行计划（严格按这个走）

------

# 🗓 Day 1–2（16h）

```text
✔ AxisViewModel设计
✔ 单元测试
✔ tick机制打通
```

------

# 🗓 Day 3（8h）

```text
✔ UI初版（能操作）
✔ 基础绑定
```

------

# 🗓 Day 4（8h）

```text
✔ UI完善
✔ 输入校验
✔ 状态显示
```

------

# 🗓 Day 5（8h）

```text
✔ 系统验证
✔ 修复bug
✔ Demo跑通
```

------

------

# 六、阶段完成定义（Definition of Done）

------

## ✅ 必须满足：

------

```text
✔ UI 可操作所有核心功能
✔ 系统行为完全正确
✔ 所有测试通过
✔ 无明显交互错误
✔ Demo 可稳定运行
```

------

------

# 七、风险与应对

------

## ⚠️ 风险1：Qt绑定问题

```text
→ 预留 0.5天buffer
```

------

## ⚠️ 风险2：状态不同步

```text
→ 强制 UI 只读 ViewModel
```

------

## ⚠️ 风险3：tick节奏问题

```text
→ 固定 tick（如 10ms）
```

------

------

# 八、阶段价值（你完成后会得到什么）

------

```text
✔ 一个可演示的工业控制系统
✔ 一个可操作的产品雏形
✔ 一套完整架构闭环
✔ 可继续扩展（多轴 / UI / 网络）
```

------

------

# 🧠 九、核心总结（最重要）

------

```text
Phase 6 的本质不是“写UI”

而是：

👉 建立“人 ↔ 控制系统”的交互闭环
```

------

------

# 🔥 最后一句话（很关键）

------

```text
你现在已经不是在“做项目”，

你是在：

👉 做一个可以被别人用的系统
```

------

------

