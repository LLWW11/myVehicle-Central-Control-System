import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import com.alientek.qmlcomponents 1.0

ApplicationWindow {
    id: window
    visible: true
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    x: 0
    y: 0
    flags: Qt.FramelessWindowHint
    title: qsTr("摄像头采集与录像")
    color: "#181b20"

    SystemUICommonApiClient {
        id: systemUICommonApiClient
        appName: "camrecorder"

        onActionCommand: function(command) {
            if (command === SystemUICommonApiClient.Show) {
                window.x = 0
                window.y = 0
                window.show()
            } else if (command === SystemUICommonApiClient.Quit) {
                window.exitSafely()
            }
        }
    }

    /**
     * 将录像秒数格式化为 HH:MM:SS。
     */
    function formatDuration(totalSeconds) {
        var hours = Math.floor(totalSeconds / 3600)
        var minutes = Math.floor((totalSeconds % 3600) / 60)
        var seconds = totalSeconds % 60
        return (hours < 10 ? "0" : "") + hours + ":"
                + (minutes < 10 ? "0" : "") + minutes + ":"
                + (seconds < 10 ? "0" : "") + seconds
    }

    /**
     * 在非录像状态执行后端清理，然后关闭窗口。
     */
    function exitSafely() {
        if (cameraController.requestExit())
            Qt.quit()
    }

    onClosing: {
        close.accepted = cameraController.requestExit()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Rectangle {
            id: previewFrame
            Layout.preferredWidth: 640
            Layout.minimumWidth: 640
            Layout.maximumWidth: 640
            Layout.preferredHeight: 480
            Layout.minimumHeight: 480
            Layout.maximumHeight: 480
            Layout.alignment: Qt.AlignVCenter
            color: "#050607"
            border.color: cameraController.recording ? "#ef4444" : "#3b424c"
            border.width: cameraController.recording ? 3 : 1

            // 使用 Pad 且固定为 640×480，确保画面既不拉伸也不铺满窗口。
            Image {
                id: previewImage
                anchors.fill: parent
                source: cameraController.cameraOpen
                        ? "image://camera/preview?revision=" + cameraController.previewRevision
                        : ""
                cache: false
                asynchronous: false
                fillMode: Image.Pad
                horizontalAlignment: Image.AlignHCenter
                verticalAlignment: Image.AlignVCenter
                visible: cameraController.cameraOpen
            }

            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: !cameraController.cameraOpen

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: cameraController.cameraBusy
                    visible: running
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: cameraController.cameraBusy
                          ? qsTr("正在启动摄像头…")
                          : qsTr("摄像头未开启")
                    color: "#cbd5e1"
                    font.pixelSize: 22
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("预览尺寸：640 × 480")
                    color: "#7f8b99"
                    font.pixelSize: 15
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 12
                width: recRow.width + 18
                height: 32
                radius: 4
                color: "#cc17191d"
                visible: cameraController.recording || cameraController.stopping

                Row {
                    id: recRow
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 10
                        height: 10
                        radius: 5
                        color: cameraController.stopping ? "#f59e0b" : "#ef4444"
                    }

                    Label {
                        text: cameraController.stopping
                              ? qsTr("保存中")
                              : qsTr("REC  ") + window.formatDuration(
                                    cameraController.recordingSeconds)
                        color: "white"
                        font.bold: true
                        font.pixelSize: 15
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 480
            Layout.minimumHeight: 480
            Layout.maximumHeight: 480
            Layout.alignment: Qt.AlignVCenter
            radius: 6
            color: "#242a32"
            border.color: "#39424e"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Label {
                    text: qsTr("摄像控制")
                    color: "white"
                    font.pixelSize: 20
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                Button {
                    Layout.fillWidth: true
                    text: cameraController.cameraOpen
                          ? qsTr("关闭摄像头") : qsTr("开启摄像头")
                    enabled: !cameraController.cameraBusy
                             && !cameraController.recording
                             && !cameraController.stopping
                    onClicked: {
                        if (cameraController.cameraOpen)
                            cameraController.closeCamera()
                        else
                            cameraController.openCamera()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Mode1")
                        highlighted: cameraController.mode === 1
                        enabled: !cameraController.recording
                                 && !cameraController.stopping
                        onClicked: cameraController.setMode(1)
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Mode2")
                        highlighted: cameraController.mode === 2
                        enabled: !cameraController.recording
                                 && !cameraController.stopping
                        onClicked: cameraController.setMode(2)
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: cameraController.mode === 1
                          ? qsTr("Mode1：仅采集和显示")
                          : qsTr("Mode2：预览并允许录像")
                    color: cameraController.mode === 1 ? "#60a5fa" : "#f59e0b"
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 14
                }

                Button {
                    Layout.fillWidth: true
                    text: cameraController.recording
                          ? qsTr("停止录像")
                          : (cameraController.stopping
                             ? qsTr("正在保存…") : qsTr("开始录像"))
                    visible: cameraController.mode === 2
                    enabled: cameraController.cameraOpen
                             && !cameraController.cameraBusy
                             && !cameraController.stopping
                             && (cameraController.recording
                                 || cameraController.usbReady)
                    highlighted: cameraController.recording
                    onClicked: {
                        if (cameraController.recording)
                            cameraController.stopRecording()
                        else
                            cameraController.startRecording()
                    }
                }

                Button {
                    Layout.fillWidth: true

                    text: cameraController.rtspEnabled
                          ? qsTr("关闭 RTSP 推流") : qsTr("开启 RTSP 推流")

                    enabled: cameraController.cameraOpen
                             && !cameraController.cameraBusy
                             && !cameraController.recording
                             && !cameraController.stopping

                    onClicked: {
                        if (cameraController.rtspEnabled)
                            cameraController.stopRtsp()
                        else
                            cameraController.startRtsp()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: cameraController.rtspEnabled
                          ? cameraController.rtspStatus
                          : qsTr("RTSP：未开启")
                    color: cameraController.rtspEnabled ? "#4ade80" : "#93a1b2"
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: 12
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#46505d"
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 3

                    Label { text: qsTr("设备"); color: "#93a1b2" }
                    Label { text: "/dev/video1"; color: "#e5e7eb" }
                    Label { text: qsTr("采集"); color: "#93a1b2" }
                    Label { text: "640×480 / 30 FPS"; color: "#e5e7eb" }
                    Label { text: qsTr("录像"); color: "#93a1b2" }
                    Label { text: "MJPEG / 15 FPS"; color: "#e5e7eb" }
                    Label { text: qsTr("质量"); color: "#93a1b2" }
                    Label { text: "JPEG 85"; color: "#e5e7eb" }
                    Label { text: qsTr("推流"); color: "#93a1b2" }
                    Label { text: "MJPEG/RTP · :8554 /camera"; color: "#e5e7eb" }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: cameraController.usbReady
                              ? qsTr("U盘：可用") : qsTr("U盘：不可用")
                        color: cameraController.usbReady ? "#4ade80" : "#f87171"
                        font.bold: true
                    }

                    Button {
                        text: qsTr("刷新")
                        enabled: !cameraController.recording
                                 && !cameraController.stopping
                        onClicked: cameraController.refreshUsbStatus()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: cameraController.usbStatus
                    color: "#b5c0ce"
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("状态：") + cameraController.statusMessage
                    color: "#d5dbe3"
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                }

                Label {
                    Layout.fillWidth: true
                    text: cameraController.errorMessage
                    color: "#fb7185"
                    visible: text.length > 0
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                }

                Label {
                    Layout.fillWidth: true
                    text: cameraController.lastSavedPath.length > 0
                          ? qsTr("已保存：") + cameraController.lastSavedPath : ""
                    color: "#86efac"
                    visible: text.length > 0
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: 11
                }

                Item { Layout.fillHeight: true }

            }
        }
    }
}
