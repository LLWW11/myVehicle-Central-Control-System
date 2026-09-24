import QtQuick 2.12
import QtQuick.Controls 2.5
import com.alientek.qmlcomponents 1.0
import QtQuick.Layouts 1.12
Item {
    id: musicLayout
    anchors.fill: parent
    property bool foregroundAllowed: true
    onForegroundAllowedChanged: musicPlayer.setForegroundAllowed(foregroundAllowed)
    property color backgroundColor: "#202735"
    signal playBtnSignal()
    signal previousBtnSignal()
    signal nextBtnSignal()
    property int progress_maximumValue: 0
    property bool progress_pressed: false
    property int progress_value: 0
    property string errorText: ""
    // 以 1024 像素宽度为设计基准，统一控制界面元素的缩放比例。
    property real scaleFactor: musicLayout.width / 1024

    // 将毫秒时长转换为播放器使用的“分:秒”文本。
    function currentMusicTime(time) {
        var sec = Math.floor(time / 1000);
        var hours = Math.floor(sec / 3600);
        var minutes = Math.floor((sec - hours * 3600) / 60);
        var seconds = sec - hours * 3600 - minutes * 60;
        var hh, mm, ss;
        if(hours.toString().length < 2)
            hh = "0" + hours.toString();
        else
            hh = hours.toString();
        if(minutes.toString().length < 2)
            mm="0" + minutes.toString();
        else
            mm = minutes.toString();
        if(seconds.toString().length < 2)
            ss = "0" + seconds.toString();
        else
            ss = seconds.toString();
        return /*hh+":"*/ + mm + ":" + ss
    }

    onPlayBtnSignal: {
        if (music_playlistModel.count === 0)
            return
        if (musicPlayer.playbackState === MusicEngine.PlayingState)
            musicPlayer.pause()
        else {
            if (musicPlayer.source === "")
                musicPlayer.source = music_playlistModel.getcurrentPath()
            musicPlayer.play()
        }
    }

    onPreviousBtnSignal: {
        selectAndPlay(music_playlistModel.previousIndex())
    }

    onNextBtnSignal: {
        selectAndPlay(music_playlistModel.nextIndex())
    }

    // 一次用户操作只选择并播放一次。
    function selectAndPlay(index) {
        if (index < 0 || index >= music_playlistModel.count)
            return
        music_playlistModel.currentIndex = index
        musicPlayer.source = music_playlistModel.getcurrentPath()
        musicPlayer.play()
    }

    LyricModel {
        id: music_lyricModel
    }

    PlayListModel {
        id: music_playlistModel
        Component.onCompleted: {
            addPresets()
        }
    }

    Connections {
        target: musicPlayer
        onPositionChanged: {
            if(!progress_pressed) {
                progress_value = musicPlayer.position
                music_lyricModel.getIndex(progress_value)
            }
        }
        onDurationChanged: {
            progress_maximumValue = musicPlayer.duration
        }
    }
    Connections {
        target: music_playlistModel
        onCurrentIndexChanged: {
            playList.music_currentIndex = music_playlistModel.currentIndex
        }
    }

    MusicEngine {
        id: musicPlayer
        onLyricReady: {
            music_lyricModel.setLrc(text)
        }
        onErrorMessage: {
            musicLayout.errorText = message
            errorTimer.restart()
        }
        onNaturalPlaybackEnded: {
            selectAndPlay(music_playlistModel.nextIndex())
        }
    }

    Timer {
        id: errorTimer
        interval: 5000
        repeat: false
        onTriggered: musicLayout.errorText = ""
    }

    Component.onCompleted: {
        musicPlayer.setForegroundAllowed(foregroundAllowed)
    }

    // 使用固定纯色作为页面背景，避免封面模糊带来的额外渲染开销。
    Rectangle {
        anchors.fill: parent
        color: musicLayout.backgroundColor
    }

    Rectangle {
        z: 20
        visible: musicLayout.errorText.length > 0
        anchors.top: parent.top
        anchors.topMargin: 12 * scaleFactor
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 40, errorMessageText.implicitWidth + 30)
        height: errorMessageText.implicitHeight + 18
        radius: 8
        color: "#b04040"
        Text {
            id: errorMessageText
            anchors.centerIn: parent
            text: musicLayout.errorText
            color: "white"
            font.pixelSize: 17 * scaleFactor
            wrapMode: Text.Wrap
            width: Math.min(implicitWidth, musicLayout.width - 70)
        }
    }

    RowLayout {
        z: 10
        anchors.fill: parent
        spacing: 25
        AlbumImage {
            id: art_album
            Layout.leftMargin: 50
            Layout.preferredWidth: musicLayout.height / 2.5
            Layout.preferredHeight: musicLayout.height / 2.5
            radius: 15
            source: musicPlayer.coverPath.length > 0
                    ? musicPlayer.coverPath
                    : "qrc:/images/default.jpg"
            visible: true
        }
        Item {
            Layout.rightMargin: 50 //距离右边框50px
            Layout.fillWidth: true
            Layout.fillHeight: true
            Item {
                anchors.fill: parent
                PlayPanel {id: playPanel}
                PlayList {
                    id: playList
                    visible: !playPanel.visible
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height + 25
                }
            }
        }
    }

    Button {
        id: playListBt
        z: 11
        width: 64 * scaleFactor
        height: width
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        opacity: playListBt.pressed ? 0.5 : 1.0
        checkable: true
        checked: false
        background: Rectangle {
            width: 50 * scaleFactor
            height: width
            radius: 10
            color: playListBt.checked ? "#94a2c6" : "transparent"
            Image {
                source: "qrc:/icons/btn_playlist.png"
                width: 30 * scaleFactor
                height: width
                anchors.centerIn: parent
            }
        }
        onCheckedChanged: {
            playPanel.visible = !playListBt.checked
        }
    }
}
