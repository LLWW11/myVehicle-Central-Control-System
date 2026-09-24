import QtQuick 2.0
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.5
import com.alientek.qmlcomponents 1.0
ColumnLayout {
    anchors.fill: parent
    id: playPanel
    Connections {
        target: musicLayout
        onProgress_maximumValueChanged:  {
            progress_control.to = progress_maximumValue
        }

        onProgress_valueChanged: {
            progress_control.value = progress_value
        }
    }

    Lyric {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    Item {
        Layout.preferredHeight: parent.height / 5
        Layout.fillWidth: true
        Slider {
            id: progress_control
            stepSize: 100
            width: parent.width
            anchors.centerIn: parent
            height: 40 * scaleFactor
            from: 0
            live: true
            onPressedChanged: {
                if (progress_control.pressed) {
                    progress_pressed = true
                } else if (progress_pressed) {
                    progress_pressed = false
                    if (musicPlayer.seekable)
                        musicPlayer.seek(Math.round(progress_control.value))
                }
            }
            onValueChanged: {
                if (progress_control.pressed)
                    music_lyricModel.findIndex(value)
            }
            background: Rectangle {
                x: progress_control.leftPadding
                y: progress_control.topPadding + progress_control.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 8
                width: progress_control.availableWidth
                height: 15 * scaleFactor
                radius: height / 2
                color: "#88DDDDDD"

                Rectangle {
                    width: progress_control.visualPosition * parent.width
                    height: parent.height
                    color: "white"
                    radius: height / 2
                }
            }

            handle: Item{ }

            Text{
                anchors.left: progress_control.left
                anchors.leftMargin: 5
                anchors.top: progress_control.bottom
                anchors.topMargin: -5 * scaleFactor
                anchors.verticalCenter: progress_control.verticalCenter
                text: currentMusicTime(progress_value)
                color: "#D0D0D0"
                font.pixelSize: 20 * scaleFactor
            }

            Text{
                anchors.right: progress_control.right
                anchors.top: progress_control.bottom
                anchors.topMargin: -5 * scaleFactor
                anchors.rightMargin: 5
                anchors.verticalCenter: progress_control.verticalCenter
                text: currentMusicTime(progress_maximumValue)
                color: "#D0D0D0"
                font.pixelSize: 20 * scaleFactor
            }
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: parent.height / 5
        RowLayout {
            width: parent.width
            height: parent.height / 2
            anchors.centerIn: parent
            spacing: 30 * scaleFactor
            Button {
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                id: btn_previous
                background: Item {
                    anchors.fill: parent
                    Image {
                        width: 50 * scaleFactor
                        height: width
                        source: "qrc:/icons/btn_previous.png"
                        anchors.centerIn: parent
                    }
                    opacity: btn_previous.pressed ? 0.5 : 1.0
                }
                onClicked: previousBtnSignal()
            }

            Button {
                id: btn_play
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                background: Item {
                    anchors.fill: parent
                    Image {
                        width: 50 * scaleFactor
                        height: width
                        source: musicPlayer.playbackState === MusicEngine.PlayingState ? "qrc:/icons/btn_pause.png" : "qrc:/icons/btn_play.png"
                        anchors.centerIn: parent
                    }
                }
                onClicked: playBtnSignal()
            }

            Button {
                id: btn_next
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                background: Item {
                    anchors.fill: parent
                    Image {
                        source: "qrc:/icons/btn_next.png"
                        anchors.centerIn: parent
                        width: 50 * scaleFactor
                        height: width
                    }
                    opacity: btn_next.pressed ? 0.5 : 1.0
                }
                onClicked: nextBtnSignal()
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                id: btn_volumedownBt
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: btn_volumedownBt.pressed ? 0.5 : 1.0
                background: Image {
                    source: "qrc:/icons/btn_volumedown.png"
                    width: 40 * scaleFactor
                    height: width
                    anchors.centerIn: parent
                }
                onClicked:{
                    volume_control.value -= 0.05
                }
            }
            Slider {
                id: volume_control
                Layout.preferredWidth: 180 * scaleFactor
                Layout.preferredHeight: 40 * scaleFactor
                from: 0
                live: true
                to: 1
                stepSize: 0.01
                value: musicPlayer.volume
                onValueChanged: {
                    musicPlayer.volume = value
                }
                background: Rectangle {
                    x: volume_control.leftPadding
                    y: volume_control.topPadding + volume_control.availableHeight / 2 - height / 2
                    implicitWidth: 200
                    implicitHeight: 8
                    width: volume_control.availableWidth
                    height: 15 * scaleFactor
                    radius: height / 2
                    color: "#88DDDDDD"

                    Rectangle {
                        width: volume_control.visualPosition * parent.width
                        height: parent.height
                        color: "white"
                        radius: height / 2
                    }
                }

                handle: Item{ }
            }
            Button {
                id: btn_volumeupBt
                Layout.preferredHeight: 50
                Layout.preferredWidth: 50
                opacity: btn_volumeupBt.pressed ? 0.5 : 1.0
                background: Image {
                    source: "qrc:/icons/btn_volumeup.png"
                    width: 40 * scaleFactor
                    height: width
                    anchors.centerIn: parent
                }
                onClicked:{
                    volume_control.value += 0.05
                }
            }
        }
    }
}
