/******************************************************************
Copyright 漏 Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         Page1.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2024-04-07
* @link          http://www.openedv.com/forum.php
*******************************************************************/
import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import com.alientek.qmlcomponents 1.0

Item {
    id: page1

    width:desktop.width
    height:desktop.height
    property bool colonVisible: true
    SystemTime {
        id: localTime
    }
    Timer { 
        interval:1000; 
        repeat:true; 
        running: page1.visible; 
        onTriggered: colonVisible=!colonVisible 
        } 
    Item {
        anchors.fill: parent
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 20 * scaleFfactor
            // scaleFfactor: main.qml 定义的自适应系数 = 屏幕宽 / 1024

            // Text {
            //     text: localTime.system_time
            //     color: "#70de89"
            //     font.pixelSize: 200 * scaleFfactor
            //     font.bold: true
            //     Layout.alignment: Qt.AlignHCenter
            // }
            // ---- 大号时间（冒号 500ms 闪烁，opacity 切换宽度不变） ----
            Row {
                spacing: 0
                Layout.alignment: Qt.AlignHCenter
                Text {
                    text: localTime.system_time.length >= 5 ? localTime.system_time.substring(0, 2) : "00"
                    color: "#70de89"
                    font.pixelSize: 200 * scaleFfactor
                    font.bold: true
                }
                Text {
                    text: ":"
                    color: "#70de89"
                    font.pixelSize: 200 * scaleFfactor
                    font.bold: true
                    opacity: colonVisible ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }
                Text {
                    text: localTime.system_time.length >= 5 ? localTime.system_time.substring(3, 5) : "00"
                    color: "#70de89"
                    font.pixelSize: 200 * scaleFfactor
                    font.bold: true
                }
            }
            // ---- 星期 ----
            Text {
                text: localTime.system_week
                color: "white"
                font.pixelSize: 50 * scaleFfactor
                Layout.alignment: Qt.AlignHCenter
            }

            // ---- 日期 ----
            Text {
                text: localTime.system_date2
                color: "white"
                font.pixelSize: 50 * scaleFfactor
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
