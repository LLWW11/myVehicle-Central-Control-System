/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @projectName   key
* @brief         KeyLayout.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @link          www.openedv.com
* @date          2024-11-28
*******************************************************************/
import QtQuick 2.0
import com.alientek.qmlcomponents 1.0
Item {
    anchors.fill: parent
    KeyInputEventThread {
        id: keyInputEventThread
        onKeyEvent: {
            switch(code) {
            case Qt.Key_VolumeDown:
                if (value === true) {
                    keyImage.opacity = 0.5
                } else if ((value === false)) {
                    keyImage.opacity = 1
                }
                break;
            default:
                break;
            }
        }
    }
    Image {
        id: keyImage
        anchors.centerIn: parent
        source: "qrc:/icons/key.png"
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: keyImage.bottom
        anchors.topMargin: 25
        text: qsTr("请按下板载KEY0按键")
        font.pixelSize: 25
    }
}
