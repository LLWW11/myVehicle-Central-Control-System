
import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 2.12
import QtLocation 5.9
import QtPositioning 5.5
import com.alientek.qmlcomponents 1.0

Window {
    id: window


    readonly property int screenWidth: 1024
    readonly property int screenHeight: 600
    readonly property int mapWidth: 640
    readonly property int mapHeight: 480

    readonly property double initialLatitude: 39.9
    readonly property double initialLongitude: 116.4
    readonly property real initialZoomLevel: 16


    property double displayedLatitude: initialLatitude
    property double displayedLongitude: initialLongitude
    property real displayedZoomLevel: initialZoomLevel
    property string displayedMapType: qsTr("等待 Provider")
    property string statusMessage: qsTr("正在初始化地图……")
    property bool followLocation: true //true表示地图跟随串口定位

    width: screenWidth
    height: screenHeight
    minimumWidth: screenWidth
    minimumHeight: screenHeight
    maximumWidth: screenWidth
    maximumHeight: screenHeight
    visible: true
    x: 0
    y: 0
    flags: Qt.FramelessWindowHint
    color: "#17191C"
    title: qsTr("精简地图")

    //右侧信息面板显示
    function updateDisplayedMapInfo()
    {
        displayedLatitude = map.center.latitude
        displayedLongitude = map.center.longitude
        displayedZoomLevel = map.zoomLevel

        if (map.activeMapType)
            displayedMapType = map.activeMapType.name
    }

    //将地图中心移动到当前有效的串口定位坐标
    function centerMapOnLocation()
    {
        if (!serialLocation.valid)
            return

        map.center = QtPositioning.coordinate(
                serialLocation.latitude,
                serialLocation.longitude)

        mapInfoUpdateTimer.restart()
    }

    SystemUICommonApiClient {
        id: systemUICommonApiClient
        appName: "maplite"

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

    Plugin {
        id: osmPlugin
        name: "osm"

        PluginParameter {
            name: "osm.mapping.custom.host"
            value: "https://tile.openstreetmap.de/"
//            value: "https://tile.openstreetmap.org/"
        }
//        PluginParameter{
//            name: "osm.useragent"
//            value: "MapViewerLite/1.0"
//        }
        PluginParameter {
            name: "osm.mapping.providersrepository.disabled"
            value: true
        }

        PluginParameter {
            name: "osm.mapping.highdpi_tiles"
            value: false
        }
    }

    Rectangle {
        id: mapFrame

        width: window.mapWidth
        height: window.mapHeight
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.verticalCenter: parent.verticalCenter
        color: "black"
        clip: true

        Map {
            MapQuickItem {
                id: currentLocationMarker

                visible: serialLocation.valid

                coordinate: QtPositioning.coordinate(
                    serialLocation.latitude,
                    serialLocation.longitude)

                anchorPoint.x: marker.width / 2
                anchorPoint.y: marker.height / 2

                sourceItem: Rectangle {
                id: marker

                    width: 30
                    height: 30
                    radius: 15

                    color: "#2D9CDB"
                    border.width: 3
                    border.color: "white"
                        }
            }
            id: map

            anchors.fill: parent
            plugin: osmPlugin
            center: QtPositioning.coordinate(window.initialLatitude,
                                             window.initialLongitude)
            zoomLevel: window.initialZoomLevel
            copyrightsVisible: true

            // 只保留平移和缩放
            gesture.enabled: true
            gesture.acceptedGestures: MapGestureArea.PanGesture
                                      | MapGestureArea.PinchGesture

            /**
             * @brief 从 OSM Provider 提供的地图类型中自动选择自定义瓦片类型。
             */
            function selectCustomMapType()
            {
                for (var index = 0; index < supportedMapTypes.length; ++index) {
                    var mapType = supportedMapTypes[index]
                    var typeName = String(mapType.name).toLowerCase()

                    if (typeName.indexOf("custom") !== -1) {
                        activeMapType = mapType
                        window.statusMessage = qsTr("地图服务正常")
                        mapInfoUpdateTimer.restart()
                        return
                    }
                }

                window.statusMessage = qsTr("没有找到 CustomURLMap，请检查 OSM Provider。")
            }

            Component.onCompleted: selectCustomMapType()
            onSupportedMapTypesChanged: selectCustomMapType()

            // 只安排一次延迟刷新，不在拖动的每一帧修改信息文本
            onCenterChanged: mapInfoUpdateTimer.restart()
            onZoomLevelChanged: mapInfoUpdateTimer.restart()

            onErrorChanged: {
                if (error !== Map.NoError)
                    window.statusMessage = qsTr("地图错误：") + errorString
            }
        }
    }
        Connections 
        {
            target: serialLocation
            onLocationChanged: {
                if (window.followLocation && serialLocation.valid)
                    window.centerMapOnLocation()
                    }
        }
        Connections
        {
            target: map.gesture// 手动拖动地图时自动进入自由模式

    onPanStarted: 
    {
        window.followLocation = false
    }
   
    // 手动拖动或者缩放地图时自动进入自由模式
    onPinchStarted: 
    {
        window.followLocation = false
    }
}
    Timer {
        id: mapInfoUpdateTimer

        interval: 200
        repeat: false
        onTriggered: window.updateDisplayedMapInfo()
    }

    Rectangle {
        id: informationPanel

        anchors.left: mapFrame.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: "#24272B"

        Column {
            id: informationColumn

            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            anchors.topMargin: 30
            anchors.bottomMargin: 24
            spacing: 14

            Text {
                width: parent.width
                color: "white"
                font.pixelSize: 26
                font.bold: true
                text: qsTr("地图信息")
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#4A4F55"
            }

            Text {
                width: parent.width
                color: "white"
                font.pixelSize: 20
                // text: qsTr("纬度：") + window.displayedLatitude.toFixed(6)
                text: qsTr("纬度：") + (serialLocation.valid
                      ? serialLocation.latitude.toFixed(6)
                      : "--")
            }

            Text {
                width: parent.width
                color: "white"
                font.pixelSize: 20
                // text: qsTr("经度：") + window.displayedLongitude.toFixed(6)
                text: qsTr("经度：") + (serialLocation.valid
                      ? serialLocation.longitude.toFixed(6)
                      : "--")
            }

            Text {
                width: parent.width
                color: "white"
                font.pixelSize: 18
                text: qsTr("缩放：") + window.displayedZoomLevel.toFixed(1)
            }

            Text {
                width: parent.width
                color: "white"
                elide: Text.ElideRight
                font.pixelSize: 16
                text: qsTr("地图类型：") + window.displayedMapType
            }

            Text {
                width: parent.width
                color: "#AEB4BB"
                font.pixelSize: 16
                text: qsTr("地图区域：640 × 480")
            }

            Text {
                width: parent.width
                color: serialLocation.valid ? "#6FCF97" : "#EB5757"
                font.pixelSize: 16
                wrapMode: Text.Wrap
                text: qsTr("定位状态：") + serialLocation.statusText
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#4A4F55"
            }

            Text {
                width: parent.width
                color: window.statusMessage === qsTr("地图服务正常")
                       ? "#6FCF97" : "#EB5757"
                font.pixelSize: 16
                wrapMode: Text.Wrap
                text: qsTr("状态：") + window.statusMessage
            }

            Item {
                width: 1
                height: 10
            }

Row {
    width: parent.width
    height: 48
    spacing: 8

    // 缩小按钮
    Rectangle {
        width: 76
        height: parent.height
        radius: 6
        color: zoomOutMouseArea.pressed
               ? "#D5D8DC" : "#F5F5F5"
        border.color: "#80868B"

        Text {
            anchors.centerIn: parent
            color: "#202124"
            font.pixelSize: 28
            text: "−"
        }

        MouseArea {
            id: zoomOutMouseArea
            anchors.fill: parent

            onClicked: {
                // 手动缩放后进入自由模式
                window.followLocation = false

                map.zoomLevel = Math.max(
                            map.minimumZoomLevel,
                            Math.floor(map.zoomLevel - 1))
            }
        }
    }

    // 单次定位按钮。
    Rectangle {
        width: 76
        height: parent.height
        radius: 6

        color: !serialLocation.valid
               ? "#666A70"
               : locateMouseArea.pressed
                 ? "#D5D8DC" : "#F5F5F5"

        border.color: "#80868B"

        Text {
            anchors.centerIn: parent
            color: serialLocation.valid
                   ? "#202124" : "#B8BCC1"
            font.pixelSize: 16
            text: qsTr("定位")
        }

        MouseArea {
            id: locateMouseArea
            anchors.fill: parent
            enabled: serialLocation.valid

            // 只定位一次，不自动开启持续跟随
            onClicked: window.centerMapOnLocation()
        }
    }


    Rectangle {
        width: 76
        height: parent.height
        radius: 6

        color: followMouseArea.pressed
               ? "#1E6F9F"
               : window.followLocation
                 ? "#2D9CDB" : "#F5F5F5"

        border.color: "#80868B"

        Text {
            anchors.centerIn: parent
            color: window.followLocation
                   ? "white" : "#202124"
            font.pixelSize: 16

            text: window.followLocation
                  ? qsTr("跟随")
                  : qsTr("自由")
        }

        MouseArea {
            id: followMouseArea
            anchors.fill: parent

            onClicked: {
                window.followLocation =
                        !window.followLocation

                // 重新开启跟随时立即移动到当前位置
                if (window.followLocation)
                    window.centerMapOnLocation()
            }
        }
    }

    Rectangle {
        width: 76
        height: parent.height
        radius: 6
        color: zoomInMouseArea.pressed
               ? "#D5D8DC" : "#F5F5F5"
        border.color: "#80868B"

        Text {
            anchors.centerIn: parent
            color: "#202124"
            font.pixelSize: 28
            text: "+"
        }

        MouseArea {
            id: zoomInMouseArea
            anchors.fill: parent

            onClicked: {
                window.followLocation = false

                map.zoomLevel = Math.min(
                            map.maximumZoomLevel,
                            Math.floor(map.zoomLevel + 1))
            }
        }
    }
}

            // Text {
            //     width: parent.width
            //     color: "#7F858C"
            //     font.pixelSize: 13
            //     wrapMode: Text.Wrap
            //     text: qsTr("test")
            // }
        }
    }

}
