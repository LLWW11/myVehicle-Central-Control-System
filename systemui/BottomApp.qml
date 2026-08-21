/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         BottomApp.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2024-04-07
* @link          http://www.openedv.com/forum.php
*******************************************************************/
import QtQuick 2.0
import QtGraphicalEffects 1.12
import QtQuick.Controls 2.12
import com.alientek.qmlcomponents 1.0
Item {
    anchors.fill: parent
    ApkListModel {
        id: apkListModel
        Component.onCompleted: apkListModel.add(appCurrtentDir + "/src/"+ hostName +"/apk3.cfg")
    }
    FuzzPanel {
        id: fuzzPanel
        anchors.centerIn: bottom_appItem_parent
        width: bottom_appItem_parent.width
        height: bottom_appItem_parent.height
        target: phonebg
        visible: false
    }

    Rectangle {
        id: bottom_app_rect_mask
        clip: true
        anchors.fill: bottom_appItem_parent
        radius: bottom_appItem_parent.height / 3
        color: "black"
        visible: false
    }

    OpacityMask {
        anchors.fill: fuzzPanel
        source: fuzzPanel
        maskSource: bottom_app_rect_mask
    }

    Rectangle {
        id: bottom_app_rect
        clip: true
        anchors.fill: bottom_appItem_parent
        radius: bottom_appItem_parent.height / 3
        color: "#55f0f0f0"
        visible: true
    }

    Item {
        id: bottom_appItem_parent
        width: item_listView.contentWidth
        height: desktop.width / 12 * 1.5
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10 * scaleFfactor

        ListView {
            id: item_listView
            visible: true
            anchors.centerIn: parent
            height: desktop.width / 12 * 1.5
            width: item_listView.contentWidth
            interactive: false
            orientation: ListView.Horizontal
            currentIndex: -1
            clip: true
            snapMode: ListView.SnapOneItem
            model: apkListModel
            delegate: item_listView_delegate
            spacing: 0
        }
    }

    Component {
        id: item_listView_delegate
        Button {
            id: appButton
            width: desktop.width / 8
            height: width
            enabled: installed
            onClicked: {
                launchActivity(programName)
            }
            background: Image {
                id: appIcon
                anchors.centerIn: parent
                width: desktop.width / 12
                height: width
                source: apkIconPath
                visible: systemUICommonApiServer.currtentLauchAppName !== programName
                Rectangle {
                    radius: 20 * scaleFfactor
                    color: "#33101010"
                    anchors.fill: parent
                    visible: appButton.pressed
                }
            }

           Image {
                id: appIcon2
                anchors.centerIn: parent
                width: appIcon.width
                height: width
                source: apkIconPath
                visible: systemUICommonApiServer.coldLaunch
                Rectangle {
                    radius: 20 * scaleFfactor
                    color: "#33101010"
                    anchors.fill: parent
                    visible: appButton.pressed
                }
            }

            /*Colorize {
                id: colorize1
                anchors.fill: appIcon2
                source: appIcon2
                saturation: 0.0
                lightness: -1.0
                opacity: 0.2
                cached: true
                visible: appButton.pressed
            }*/
        }
    }
}
