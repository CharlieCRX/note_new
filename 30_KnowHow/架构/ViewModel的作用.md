# 一、先看没有 ViewModel 的情况

假设 UI 直接调用 UseCase。

```
UI
 ↓
UseCase
```

Qt代码可能是这样：

```C++
void MainWindow::onStartClicked()
{
    SyncMoveUseCase uc;

    uc.execute({x1, x2});

    statusLabel->setText("运行完成");
}
```

一开始看起来很简单。

但很快 UI 会变成这样：

```c++
void MainWindow::onStartClicked()
{
    startButton->setEnabled(false);
    statusLabel->setText("运行中");

    SyncMoveUseCase uc;

    auto result = uc.execute({x1, x2});

    if(result.success)
        statusLabel->setText("完成");
    else
        statusLabel->setText("错误");

    startButton->setEnabled(true);
}
```

问题开始出现：

UI代码里混入了：

```
运行状态
错误处理
按钮状态
业务结果
```

UI代码会越来越像这样：

```
UI + 状态机 + 业务响应
```

UI开始变得非常复杂。

# 二、ViewModel解决的是什么问题



ViewModel的作用其实只有一句话：

> **把 UI 状态管理从 UI 中拿出来。**

结构变成：

```
UI
 ↓
ViewModel
 ↓
UseCase
 ↓
Domain
```

UI只负责：

```
显示
用户输入
```

ViewModel负责：

```
状态
调用UseCase
处理结果
```

# 三、一个更真实的例子

假设界面上有：

```
位置显示
运行状态
开始按钮
停止按钮
错误信息
```

UI代码：

```C++
void MainWindow::onStartClicked()
{
    viewModel.startSyncMove(x1, x2);
}
```

UI非常干净。

------

## ViewModel

```c++
void MotionViewModel::startSyncMove(double x1, double x2)
{
    state.running = true;
    state.error = "";
    notifyUI();

    auto result = syncMoveUseCase.execute({x1,x2});

    state.running = false;

    if(!result.success)
        state.error = result.message;

    notifyUI();
}
```

ViewModel维护：

```
UI状态
```

例如：

```
running
error
position
progress
```

# 四、设备状态（Device State） 和 UI状态（UI State） 的区别

像这些：

```
当前位置
速度
伺服状态
报警
运行中
```

**本质上不是 UI 状态。**

它们是：

```
设备状态（Device State）
或
领域状态（Domain State）
```

来源是：

```
PLC / 驱动 / Domain
```

而 UI 状态是：

```
按钮是否可点击
是否显示loading
错误文字
当前选中的轴
当前页面
```

# 五、为什么需要 ViewModel？

 **UI不应该直接依赖设备接口**。

结构应该是：

```
UI
 ↓
ViewModel
 ↓
DeviceService / UseCase
 ↓
PLC / Domain
```

ViewModel在这里承担的是：

> **把设备状态转成 UI 可用的状态**

# 六、举个非常真实的例子

PLC里可能有：

```
D1000 = 当前位置
D1002 = 速度
M200  = 伺服使能
M201  = 报警
```

如果 UI 直接读取 PLC：

```
double pos = plc.read(D1000);
bool alarm = plc.read(M201);
```

那 UI 就知道了：

```
PLC
寄存器地址
通信方式
```

UI 就被污染了。

# 七、正确结构

PLC通信在底层：

```
PLCDriver
```

Domain封装成：

```C++
AxisStatus
```

例如：

```C++
struct AxisStatus
{
    double position;
    double velocity;
    bool servoOn;
    bool alarm;
};
```

然后：

```C++
StatusService
```

负责读取：

```C++
AxisStatus readAxisStatus(axisId);
```

# 八、ViewModel的作用

ViewModel接收 **设备状态**，然后整理为 **UI模型**。

例如：

```C++
class AxisViewModel
{
public:

    double position;
    double velocity;
    QString servoText;
    QString alarmText;
    bool startButtonEnabled;
};
```

更新逻辑：

```C++
void AxisViewModel::update()
{
    auto status = axisService.readStatus();

    position = status.position;
    velocity = status.velocity;

    servoText = status.servoOn ? "Servo ON" : "Servo OFF";

    alarmText = status.alarm ? "报警" : "正常";

    startButtonEnabled = status.servoOn && !status.alarm;
}
```

注意这里发生了一件事情：

```
设备状态 → UI状态
```

------

# 九、UI只负责显示

UI代码就会变得非常干净：

```C++
positionLabel = viewModel.position
velocityLabel = viewModel.velocity
servoLabel = viewModel.servoText
alarmLabel = viewModel.alarmText
startButton.enabled = viewModel.startButtonEnabled
```

UI完全不需要知道：

```
PLC
寄存器
通信
设备逻辑
```

------

# 十、一个工业UI中常见的转换

设备状态：

```
servoOn = true
alarm = false
```

UI显示：

```
绿色灯
"Servo Ready"
```

设备状态：

```
servoOn = false
alarm = true
```

UI显示：

```
红色灯
"Alarm"
```

这些转换 **绝对不应该写在 UI 里**。

应该写在：

```
ViewModel
```

------

# 十一、所以可以这样理解

工业软件里其实有 **三种状态**：

### 1️⃣ 设备状态（Device State）

来自：

```
PLC
伺服
传感器
```

例如：

```
位置
速度
报警
IO
```

------

### 2️⃣ 业务状态（Business State）

例如：

```
自动运行
回原点中
同步运动中
```

通常来自：

```
UseCase
```

------

### 3️⃣ UI状态（UI State）

例如：

```
按钮可点击
显示文字
颜色
进度条
loading
```

------

# 十二、ViewModel的真实职责

ViewModel其实就是做 **状态整合器**：

```
设备状态
      ↓
业务状态
      ↓
UI状态
```

最后输出给 UI。

------

# 十三、工业控制 UI 的一个典型数据流

通常会是这样：

```
PLC
 ↓
Driver
 ↓
Domain / StatusService
 ↓
ViewModel
 ↓
UI
```

UI只负责：

```
显示
```

# 十四、一个你现在阶段很关键的理解

你刚刚其实已经意识到了：

> “轴位置这些不太像 UI 状态”

这是对的。

所以更准确说：

```
ViewModel = UI数据模型
```

它不只是 UI 状态，也包含：

```
UI需要的数据
```

但它会 **屏蔽底层复杂性**。