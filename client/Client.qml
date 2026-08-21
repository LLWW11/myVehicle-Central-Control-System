/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         Client.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2024-09-10
* @link          http://www.openedv.com/forum.php
*******************************************************************/
import QtQuick 2.0
import com.alientek.qmlcomponents 1.0
Item {
    id: client
    property string programmerName
    anchors.fill: parent
    SystemUICommonApiClient {
        id: systemUICommonApiClient
        appName: programmerName
        onActionCommand: {
            if (cmd === SystemUICommonApiClient.Show) {
                //systemUICommonApiClient.requestVisibilityChange(SystemUICommonApiClient.Hide)
                systemUICommonApiClient.askSystemUItohideOrShow(SystemUICommonApiClient.Hide)
                window.x = 0
                window.y = 0
                appMainBody.visible = true
                window.show()
            }
            if (cmd === SystemUICommonApiClient.Quit)
                Qt.quit()
        }
    }


    AppMainBody {
        anchors.fill: parent
        id: appMainBody
        visible: true
    }

}
