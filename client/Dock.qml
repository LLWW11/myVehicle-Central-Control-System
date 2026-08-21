/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         Dock.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2024-04-07
* @link          http://www.openedv.com/forum.php
*******************************************************************/
import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
Item {
    signal backSignal()
    signal closeSignal()
    id: dock
    width: 100
    height: 50
    anchors.top: parent.top
    anchors.topMargin: 2
    anchors.right: parent.right
    anchors.rightMargin: 2
    RowLayout {
        anchors.fill: parent
        Button {
            id: backBt
            Layout.preferredWidth: 50
            Layout.preferredHeight: 50
            opacity: backBt.pressed ? 0.5 : 1.0
            background: Item {
                anchors.fill: parent
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width / 2
                    height: width
                    color: "#feb53d"
                    radius: height
                    border.color: "gray"
                    border.width: 1
                }
            }
            onClicked: {
                backSignal()
            }
        }

        Button {
            id: closeBt
            Layout.preferredWidth: 50
            Layout.preferredHeight: 50
            opacity: closeBt.pressed ? 0.5 :1.0
            background: Item {
                anchors.fill: parent
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width / 2
                    height: width
                    color: "#ff5653"
                    radius: height
                    border.color: "gray"
                    border.width: 1
                }
            }
            onClicked: {
               closeSignal()
            }
        }
    }
}
