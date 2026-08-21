/******************************************************************
Copyright 2022 Guangzhou ALIENTEK Electronincs Co.,Ltd. All rights reserved
Copyright © Deng Zhimao Co., Ltd. 1990-2030. All rights reserved.
* @projectName   qwifi
* @brief         CustomSwitch.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2022-12-06
* @link          http://www.openedv.com/forum.php
*******************************************************************/
import QtQuick 2.2
import QtQuick.Controls 2.5

Switch {
    id: customSwitch
    property color checkedColor: "#0ACF97"
    property bool switchEnable: true

    indicator: Rectangle {
        width: parent.width
        height: parent.height
        radius: height / 2
        color: customSwitch.checked ? checkedColor : "#f0f0f0"
        border.width: 2
        border.color: customSwitch.checked ? checkedColor : "#E5E5E5"
        enabled: switchEnable

        Image {
            id: handle_cion
            x: customSwitch.checked ? parent.width - width - 2 : 2
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height
            width: height
            LayoutMirroring.enabled: true
            LayoutMirroring.childrenInherit: true
            source:customSwitch.checked ? "qrc:/icons/slider_handle_style.png" : "qrc:/icons/slider_handle_style_pressed.png"
            Behavior on x {
                NumberAnimation { duration: 50 }
            }
        }
    }
}
