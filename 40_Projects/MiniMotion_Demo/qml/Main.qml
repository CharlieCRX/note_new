import QtQuick

Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("工业电机控制 Demo")

    // 像原生组件一样引用你写的文件
    MotorDashboard {
        anchors.centerIn: parent
        // 你甚至可以在这里覆盖初始值
//        currentPos: 300
    }
}
