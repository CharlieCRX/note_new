import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    width: 600
    height: 300
    color: "#f4f4f4"
    border.color: "#ddd"
    radius: 8

    // --- 模拟 C++ 接口 (Mock Data) ---
    property real physicalMin: 0.0
    property real physicalMax: 1000.0
    property real softLimitMin: 200.0
    property real softLimitMax: 800.0
    property real currentPos: 450.0
    property bool hasError: false

    // --- 内部逻辑控制 ---
    function requestMove(target) {
        if (isNaN(target) || target < softLimitMin || target > softLimitMax) {
            hasError = true
            return false
        }
        hasError = false
        currentPos = target
        return true
    }

    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 30 // 增加间距，布局更美观

        Text {
            text: "电机运动监控看板"
            font.pixelSize: 20
            font.bold: true
        }

        // --- 可视化轨道区域 ---
        Item {
            width: parent.width
            height: 60

            Rectangle {
                id: track
                width: parent.width
                height: 12
                color: "#e0e0e0"
                radius: 6
                anchors.verticalCenter: parent.verticalCenter

                // 左侧软限位阴影
                Rectangle {
                    width: (root.softLimitMin / root.physicalMax) * track.width
                    height: parent.height
                    color: "#ffcccc"
                    opacity: 0.6
                }

                // 右侧软限位阴影
                Rectangle {
                    x: (root.softLimitMax / root.physicalMax) * track.width
                    width: track.width - x
                    height: parent.height
                    color: "#ffcccc"
                    opacity: 0.6
                }

                // 当前位置游标
                Rectangle {
                    id: cursor
                    width: 4
                    height: 30
                    color: root.hasError ? "red" : "#2196F3"
                    anchors.verticalCenter: parent.verticalCenter
                    // 映射位置
                    x: (root.currentPos / root.physicalMax) * track.width - width/2

                    Behavior on x { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
                }
            }
        }

        // --- 控制交互区域 ---
        // 注意：删除了这里的 anchors.verticalCenter
        Row {
            spacing: 15

            TextField {
                id: targetInput
                placeholderText: "输入目标位置..."
                // 限制只能输入数字
                validator: DoubleValidator { bottom: root.physicalMin; top: root.physicalMax }
            }

            Button {
                text: "执行移动"
                onClicked: root.requestMove(parseFloat(targetInput.text))
            }

            Text {
                // 垂直居中于 Row 内部
                anchors.verticalCenter: parent.verticalCenter
                text: root.hasError ? "⚠️ 目标超出限位!" : "状态正常"
                color: root.hasError ? "red" : "green"
                font.bold: root.hasError
            }
        }
    }
}
