/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         main.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2024-04-07
* @link          http://www.openedv.com/forum.php
*******************************************************************/
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import com.alientek.qmlcomponents 1.0
Window {  //顶层窗口
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    visible: true
    property real scaleFfactor: desktop.width / 1024
    //flags: Qt.FramelessWindowHint
    x: 0
    y: 0
    id: desktop

    // 常驻 SystemUI 直接监听板载 KEY0，不依赖当前前台窗口的键盘焦点
    KeyInputEventThread {
        id: desktopKeyInput
        onKeyEvent: function(code, pressed) {
            // 只在应用位于前台且 KEY0 释放时触发一次返回操作
            if (!pressed && code === Qt.Key_VolumeDown && !rootItem.enabled)
                systemUICommonApiServer.returnToDesktop()
        }
    }

    SystemUICommonApiServer {
        id: systemUICommonApiServer
        onRequestVisibilityChange: function(action) {
            if (action === SystemUICommonApiServer.Hide) {
                rootItem.enabled = false
                desktop.x = Screen.desktopAvailableWidth
                desktop.y = Screen.desktopAvailableHeight
                desktop.hide()
            }
            if (action === SystemUICommonApiServer.Show) {
                rootItem.enabled = true
                desktop.x = 0
                desktop.y = 0
                desktop.show()
                // indicatorShowTimer.start()
                systemUICommonApiServer.currtentLauchAppName = ""
            }
        }
        onCurrtentLauchAppNameChanged: {
            if (systemUICommonApiServer.currtentLauchAppName === "null")
                systemUICommonApiServer.requestVisibilityChange(SystemUICommonApiServer.Show)
        }
    }

    Item {
        id: rootItem
        anchors.fill: parent
        Image {
            id: phonebg
            anchors.centerIn: parent
            height: parent.height
            width: parent.width
            fillMode: Image.PreserveAspectCrop
            smooth: true
            source: "file://" + appCurrtentDir + "/src/ipad/ipad/ipad.png"
            Rectangle {
                anchors.fill: parent
                color: "#55404040"
            }
        }

        SwipeView {
            id: main_swipeView
            visible: true
            anchors.fill: parent
            clip: true
            Page1 {}
            Page2 {}
        }

        PageIndicator {
            id: indicator
            count: main_swipeView.count
            visible: true
            currentIndex: main_swipeView.currentIndex
            anchors.bottom: parent.bottom
            anchors.bottomMargin: scaleFfactor * 12
            anchors.horizontalCenter: parent.horizontalCenter
            delegate: indicator_delegate

            Component {
                id: indicator_delegate
                Rectangle {
                    // opacity: 1 - explainText.opacity
                    opacity:1
                    width: scaleFfactor * 6
                    height: scaleFfactor * 6
                    color: main_swipeView.currentIndex !== index  ? "gray" : "#dddddd"
                    radius: scaleFfactor * 3
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }


    function launchActivity(name) {
        rootItem.enabled = false
        // indicatorShowTimer.stop()
        systemUICommonApiServer.launchApp(name)
    }


    
}
