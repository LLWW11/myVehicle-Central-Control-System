/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @projectName   music
* @brief         MusicLayout.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @link          www.openedv.com
* @date          2024-05-04
*******************************************************************/
import QtQuick 2.12
import QtMultimedia 5.0
import QtQuick.Controls 2.5
import com.alientek.qmlcomponents 1.0
import QtQuick.Layouts 1.12
Item {
    id: musicLayout
    anchors.fill: parent
    property color backgroundColor: "#202735"
    signal playBtnSignal()
    signal previousBtnSignal()
    signal nextBtnSignal()
    property int progress_maximumValue: 0
    property bool progress_pressed: false
    property int progress_value: 0
    // 以 1024 像素宽度为设计基准，统一控制界面元素的缩放比例。
    property real scaleFactor: musicLayout.width / 1024

    // 将毫秒时长转换为播放器使用的“分:秒”文本。
    function currentMusicTime(time) {
        if (!loader.enabled)
            return
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
        if (playList.musicCount === 0)
            return
        if (music_playlistModel.currentIndex !== -1) {
            musicPlayer.source =  music_playlistModel.getcurrentPath()
            playList.music_currentIndex = music_playlistModel.currentIndex
            musicPlayer.playbackState === MediaPlayer.PlayingState ? musicPlayer.pause() : musicPlayer.play()
        }
    }

    onPreviousBtnSignal: {
        music_playlistModel.currentIndex--
        musicPlayer.play()
    }

    onNextBtnSignal: {
        if (!musicPlayer.hasAudio)
            return
        music_playlistModel.currentIndex++
        musicPlayer.play()
    }

    LyricModel {
        id: music_lyricModel
    }

    PlayListModel {
        id: music_playlistModel
        currentIndex: 0
        onCurrentIndexChanged: {
            musicPlayer.source = getcurrentPath()
            musicPlayer.play()
        }
        Component.onCompleted: {
            songsInit()
        }
    }

    Connections {
        target: musicPlayer
        onPositionChanged: {
            if (!loader.enabled)
                return
            progress_maximumValue = musicPlayer.duration
            if(!progress_pressed) {
                progress_value = musicPlayer.position
            }
        }
        onStatusChanged: {
            if (musicPlayer.status === MediaPlayer.LoadedMedia) {
                progress_maximumValue = musicPlayer.duration
            } else if (musicPlayer.status === MediaPlayer.EndOfMedia) {
                musicPlayer.autoPlay = true
                music_lyricModel.currentIndex = 0
                progress_maximumValue = 0
                progress_value = 0
                music_playlistModel.currentIndex++
            }
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
        autoPlay: false
        onLyricReady: {
            music_lyricModel.setLrc(lrcText)
        }
    }

    // 初始化播放器内置歌曲列表。
    function songsInit() {
        music_playlistModel.addPresets()
    }

    // 使用固定纯色作为页面背景，避免封面模糊带来的额外渲染开销。
    Rectangle {
        anchors.fill: parent
        color: musicLayout.backgroundColor
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
