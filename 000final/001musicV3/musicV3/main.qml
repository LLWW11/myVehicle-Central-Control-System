import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

Window {
    id: window
    visible: true
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    flags: Qt.FramelessWindowHint
    x: 0
    y: 0
    color: "transparent"

    // 窗口隐藏或最小化即停止播放；失去键盘焦点不停止。
    readonly property bool foregroundAllowed:
        visible && visibility !== Window.Minimized

    AppMainBody {
        anchors.fill: parent
        foregroundAllowed: window.foregroundAllowed
    }
}
