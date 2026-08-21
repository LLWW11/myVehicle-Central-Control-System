/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         Page2.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2024-04-07
* @link          http://www.openedv.com/forum.php
*******************************************************************/
import QtQuick 2.12
import QtQuick.Controls 2.12
import com.alientek.qmlcomponents 1.0
import QtQuick.Layouts 1.12 // 新增，卡片ColumnLayout用
Item {
    id: page2

    ApkListModel {
        id: apkListModel

        Component.onCompleted: {
            apkListModel.add(appCurrtentDir + "/src/" + hostName + "/apk2.cfg")
        }
    }
    SystemTime { id: page2Time }
    // 1024×600 屏幕使用两列两行，每个单元格为 300×270
    GridView {
        id: itemGridView
        width: 600 * scaleFfactor
        height: 540 * scaleFfactor
        anchors.left: parent.left
        anchors.leftMargin: 32 * scaleFfactor
        anchors.verticalCenter: parent.verticalCenter
        cellWidth: 300 * scaleFfactor
        cellHeight: 270 * scaleFfactor
        interactive: false
        clip: false
        model: apkListModel
        delegate: appDelegate
    }
    Item {
        id: rightInfoCard
        width: 340*scaleFfactor; 
        height: 540*scaleFfactor
        anchors.left: itemGridView.right; 
        anchors.leftMargin: 24*scaleFfactor
        anchors.verticalCenter: parent.verticalCenter
        Rectangle { 
            anchors.fill: parent; radius: 20*scaleFfactor; color:"#1AFFFFFF"; border.color:"#22FFFFFF"; border.width:1*scaleFfactor }
        ColumnLayout {
            anchors.centerIn: parent; width: parent.width - 32*scaleFfactor; spacing: 10*scaleFfactor
            Text { 
                text: page2Time.system_time; 
                color:"#70de89"; 
                font.pixelSize:80*scaleFfactor; 
                font.bold:true; 
                Layout.alignment: Qt.AlignHCenter 
                }
            Text { 
                text: page2Time.system_date2+"  "+page2Time.system_week; 
                color:"white"; 
                font.pixelSize:36*scaleFfactor; 
                opacity:0.9; Layout.alignment: Qt.AlignHCenter 
                }
            Rectangle { 
                Layout.fillWidth:true; 
                height:1*scaleFfactor; 
                color:"#33FFFFFF"; 
                Layout.topMargin:6*scaleFfactor; 
                Layout.bottomMargin:6*scaleFfactor 
                }
            Text { 
                text:
                "已安装 4 个应用"; 
                color:"white"; 
                font.pixelSize:24*scaleFfactor; 
                opacity:0.85; 
                Layout.alignment: Qt.AlignHCenter 
                }
            Text { 
                text:"← 滑动切换 · KEY0 返回"; 
                color:"white"; 
                font.pixelSize:20*scaleFfactor; 
                opacity:0.6; 
                Layout.alignment: Qt.AlignHCenter 
                }
            Rectangle { 
                Layout.fillWidth:true; 
                height:1*scaleFfactor; 
                color:"#22FFFFFF"; 
                Layout.topMargin:10*scaleFfactor 
                }
        }
    }
    Component {
        id: appDelegate

        Button {
            id: appButton
            width: itemGridView.cellWidth
            height: itemGridView.cellHeight
            padding: 0
            enabled: installed
            opacity: installed ? 1.0 : 0.35

            onClicked: launchActivity(programName)

            background: Rectangle {
                color: "transparent"
            }

            contentItem: Item {
                Column {
                    anchors.centerIn: parent
                    width: 220 * scaleFfactor
                    spacing: 6 * scaleFfactor

                    Image {
                        id: appIcon
                        width: 220 * scaleFfactor
                        height: width
                        source: apkIconPath
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }

                    Text {
                        width: parent.width
                        height: 24 * scaleFfactor
                        text: apkName
                        color: "white"
                        font.pixelSize: 22 * scaleFfactor
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 24 * scaleFfactor
                    color: appButton.pressed ? "#33101010" : "transparent"
                }
            }
        }
    }
}
