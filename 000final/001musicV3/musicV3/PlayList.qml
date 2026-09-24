
import QtQuick.Controls 2.12
import QtQuick 2.0
import com.alientek.qmlcomponents 1.0

Item {
    id: root
    property int music_currentIndex: -1
    property int musicCount: 0

    onMusic_currentIndexChanged: {
        music_listView.currentIndex = music_currentIndex
    }

    Item {
        anchors.top: parent.top
        anchors.topMargin: 25 * scaleFactor
        width: parent.width
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -10
        Item {
            width: 80
            height: 50
            anchors.left: parent.left
            anchors.leftMargin: 40
            anchors.bottom: music_listView.top
            Text {
                text: qsTr("播放列表")
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 25 * scaleFactor
                color: "white"
            }
        }
        ListView {
            id: music_listView
            visible: true
            anchors.fill: parent
            currentIndex: 0
            clip: true
            spacing: 10
            onFlickStarted: scrollBar.opacity = 1.0
            onFlickEnded: scrollBar.opacity = 0.0

            onCountChanged: {
                musicCount = music_listView.count
            }
            ScrollBar.vertical: ScrollBar {
                id: scrollBar
                width: 10
                opacity: 0.0
                onActiveChanged: {
                    active = true;
                }
                Component.onCompleted: {
                    scrollBar.active = true;
                }
                contentItem: Rectangle{
                    implicitWidth: 6
                    implicitHeight: 100
                    radius: 2
                    color: scrollBar.hovered ? "#88101010" : "#30101010"
                }
                Behavior on opacity { PropertyAnimation { duration: 500; easing.type: Easing.Linear } }
            }

            model: music_playlistModel
            delegate: Item {
                id: itembg
                width: parent.width - 10
                height: 60 * scaleFactor
                Rectangle {
                    height: parent.height
                    anchors.right: parent.right
                    anchors.left: ablum_item.left
                    radius: 5
                    color: music_listView.currentIndex === index && musicPlayer.playbackState
                           === MusicEngine.PlayingState ? "#55808a87" : "transparent"
                }
                Item {
                    id: ablum_item
                    anchors.left: parent.left
                    anchors.leftMargin: 40 * scaleFactor
                    anchors.verticalCenter: parent.verticalCenter
                    width: itembg.height
                    height: itembg.height

                    AlbumImage {
                        id: album
                        anchors.centerIn: parent
                        width: itembg.height
                        height: itembg.height
                        radius: 5
                        source: music_playlistModel.coverUrl(index) !== ""
                                ? music_playlistModel.coverUrl(index)
                                : "qrc:/images/default.jpg"
                        visible: true
                    }
                }
                Rectangle {
                    height: 1
                    anchors.left: column.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    color: "#33ffffff"
                }
                Column {
                    id: column
                    anchors.left: ablum_item.right
                    anchors.leftMargin: 20 * scaleFactor
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        id: songsname
                        text: title
                        elide: Text.ElideRight
                        anchors.leftMargin: 10 * scaleFactor
                        color: parent.ListView.isCurrentItem && musicPlayer.playbackState === MusicEngine.PlayingState ? "white" : "#D0D0D0"
                        font.pixelSize: 25 * scaleFactor
                        font.bold: parent.ListView.isCurrentItem && musicPlayer.playbackState === MusicEngine.PlayingState
                    }

                    Text {
                        id: songsauthor
                        visible: true
                        width: 200 * scaleFactor
                        height: 15 * scaleFactor
                        text: author
                        elide: Text.ElideRight
                        color: parent.ListView.isCurrentItem && musicPlayer.playbackState === MusicEngine.PlayingState ? "white" : "#D0D0D0"
                        font.pixelSize: 15 * scaleFactor
                        font.bold: parent.ListView.isCurrentItem
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: {
                        musicLayout.selectAndPlay(index)
                    }
                }
            }
        }
    }
}
