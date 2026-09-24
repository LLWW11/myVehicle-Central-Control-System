/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @projectName   desktop
* @brief         Lyric.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com
* @link          www.openedv.com
* @date          2021-09-13
*******************************************************************/

import QtQuick 2.0
import QtQuick.Controls 2.5

Item {
    id: lyric_item
    Connections {
        target: music_lyricModel
        onCurrentIndexChanged: {
            music_lyric.currentIndex = music_lyricModel.currentIndex
        }
    }
    ListView {
        anchors.fill: parent
        id: music_lyric
        spacing: 10
        clip: true
        visible: true
        highlightRangeMode: ListView.StrictlyEnforceRange
        preferredHighlightBegin: (lyric_item.height - 40 * scaleFactor) / 2
        preferredHighlightEnd: (lyric_item.height - 40 * scaleFactor) / 2
        highlight: Rectangle {
            color: Qt.rgba(0, 0, 0, 0)
            Behavior on y {
                SmoothedAnimation {
                    duration: 300
                }
            }
        }
        model: music_lyricModel
        delegate: Rectangle {
            visible: true
            width: lyric_item.width
            height: 40 * scaleFactor
            color: Qt.rgba(0,0,0,0)
            Text {
                visible: true
                anchors.centerIn: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: "   " + textLine
                color: parent.ListView.isCurrentItem ? "white" : "#eedddddd"
                font.pixelSize: parent.ListView.isCurrentItem ? 25 * scaleFactor : 20 * scaleFactor
                Behavior on font.pixelSize { PropertyAnimation { duration: 200; easing.type: Easing.Linear } }
                font.bold: parent.ListView.isCurrentItem
                font.family: "Montserrat Light"
            }
            MouseArea {
                anchors.fill: parent
            }
        }
    }
}
