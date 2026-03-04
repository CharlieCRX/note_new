### 📝 QML 需要的接口清单 (ViewModel Contract)

#### 1. 状态属性 (Properties)

这些是 QML 需要实时读取并绑定到界面的数据，通常由 PLC 传来或由逻辑层计算得出：

- **`physicalMin` / `physicalMax` (real):** 电机物理轨道的最小/最大绝对位置（用于 UI 绘制比例尺的基础，比如 0.0 到 1000.0）。
- **`softLimitMin` / `softLimitMax` (real):** 用户或系统设置的软限位（例如 100.0 到 900.0）。
- **`currentPos` (real):** 电机的当前实时绝对位置。
- **`hasError` (bool):** 系统是否处于报警状态（例如目标位置越界）。
- **`errorMessage` (string):** 具体的报警提示文本。

#### 2. 操作指令 (Methods)

这些是用户在界面上点击按钮后，QML 需要调用 C++ 去执行的动作：

- **`requestMove(real targetPosition)`:** 请求移动到目标绝对位置。C++ 收到后会判断是否在软限位内，如果在，就发 Modbus 指令给 PLC。
- **`emergencyStop()`:** 停止电机运动（工控必备）。
- **`clearAlarm()`:** 复位/清除当前报警状态。

#### 3. 事件通知 (Signals)

这是 C++ 主动通知 QML 发生的瞬发事件（配合弹窗或动画使用）：

- **`limitTriggered()`:** 触发了限位报警时发送。
- **`moveCompleted()`:** 电机成功到达目标位置。

------

### 🛠️ 如何在 QML 中模拟这些接口？

在写具体的 UI 组件之前，我们可以在你的主文件或者组件根节点把这些属性写出来。这样你调整数值时，界面就能跟着实时变化了。

你可以新建一个 `MotorDashboard.qml` 练手：

```css
import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: 800
    height: 400

    // ==========================================
    // 1. 模拟 C++ ViewModel 注入的接口 (Mock 数据)
    // ==========================================
    property real physicalMin: 0.0
    property real physicalMax: 1000.0
    
    property real softLimitMin: 200.0
    property real softLimitMax: 800.0
    
    property real currentPos: 500.0
    
    property bool hasError: false
    property string errorMessage: ""

    // ==========================================
    // 2. 模拟 C++ 的操作方法
    // ==========================================
    function requestMove(target) {
        if (target < softLimitMin || target > softLimitMax) {
            hasError = true
            errorMessage = "错误：目标位置超出软限位！"
            console.log(errorMessage)
            return false
        }
        hasError = false
        errorMessage = ""
        currentPos = target // 模拟电机瞬间到达目标位置
        console.log("电机移动至: " + target)
        return true
    }

    function clearAlarm() {
        hasError = false
        errorMessage = ""
    }

    // ==========================================
    // 3. 你的 UI 绘制区域
    // ==========================================
    
    // 背景、滑块、报警文本、输入框等可以写在这里...
    // 它们直接绑定上面的 property 即可！
    
    Text {
        anchors.centerIn: parent
        text: root.hasError ? root.errorMessage : "当前位置: " + root.currentPos
        color: root.hasError ? "red" : "black"
        font.pixelSize: 24
    }
}
```

有了这个骨架，你的 QML 就完全独立了。后期接入 C++ 时，只需要把这些 `property` 删掉，替换成 C++ 注册的上下文对象（比如 `motorViewModel.currentPos`）即可，UI 层的代码几乎不需要改动。