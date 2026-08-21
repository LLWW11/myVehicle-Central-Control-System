/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         main.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2024-09-05
* @link          http://www.openedv.com/forum.php
*******************************************************************/

import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import com.alientek.qmlcomponents 1.0

Window {
    id: window
    visible: true
    width: Screen.desktopAvailableWidth
    height: Screen.desktopAvailableHeight
    x: 0
    y: 0
    flags: Qt.FramelessWindowHint
    title: qsTr("ALIENTEK 天气预报")

    SystemUICommonApiClient {
        id: systemUICommonApiClient
        appName: "weather"

        onActionCommand: function(command) {
            if (command === SystemUICommonApiClient.Show) {
                window.x = 0
                window.y = 0
                window.show()
            } else if (command === SystemUICommonApiClient.Quit) {
                Qt.quit()
            }
        }
    }

    // Weather 业务对象放在 WeatherLayout 内部实例化
    // (QML 的 id 作用域是文件级,跨文件不可见,所以 Weather 必须在使用它的文件内创建)
    WeatherLayout {
        anchors.fill: parent
    }

}
