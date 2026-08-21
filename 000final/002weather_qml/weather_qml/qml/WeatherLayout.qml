/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @brief         WeatherLayout.qml
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @date          2024-09-05
* @link          http://www.openedv.com/forum.php
*******************************************************************/

import QtQuick 2.12
import QtQuick.Controls 2.12
import com.alientek.qmlcomponents 1.0

Item {
    id: root
    width: 1024
    height: 600

    // ---- 数据模型(由 Weather 信号填充)----
    property var liveData: ({})           // 当前天气 QVariantMap
    property var forecastData: ({})       // 预报 QVariantMap(含 days 数组)
    property string cityName: ""

    // ---- 业务对象(在 WeatherLayout 内部实例化,Connections.target 才能解析 weather id)----
    Weather {
        id: weather
    }

    // ---- 工具函数:中文天气文字 → 图标号(对齐 widget.cpp weatherIconNumber)----
    function weatherIconNumber(weatherText) {
        if (!weatherText || weatherText.length === 0) return "3"
        // 有雨 没雷 没雪
        if (weatherText.indexOf("雨") >= 0
                && weatherText.indexOf("雷") < 0
                && weatherText.indexOf("雪") < 0) {
            if (weatherText.indexOf("小雨") >= 0)  return "6"
            if (weatherText.indexOf("中雨") >= 0)  return "1"
            if (weatherText.indexOf("大雨") >= 0)  return "4"
            if (weatherText.indexOf("暴雨") >= 0)  return "8"
            if (weatherText.indexOf("阵雨") >= 0)  return "11"
        }
        if (weatherText.indexOf("雪") >= 0) {
            if (weatherText.indexOf("雨夹雪") >= 0) return "12"
            return "5"
        }
        if (weatherText.indexOf("雷") >= 0)      return "10"
        if (weatherText.indexOf("晴") >= 0)      return "7"
        if (weatherText.indexOf("多云") >= 0)    return "3"
        if (weatherText.indexOf("雾") >= 0
                || weatherText.indexOf("霾") >= 0) return "2"
        if (weatherText.indexOf("尘") >= 0
                || weatherText.indexOf("沙") >= 0) return "9"
        return "3"   // 兜底
    }

    // 星期几转换(高德返回 1-7 数字 → 中文字,1=周一)
    function weekText(weekNum) {
        var arr = ["一", "二", "三", "四", "五", "六", "日"]
        var n = parseInt(weekNum, 10)
        if (isNaN(n) || n < 1 || n > 7) return ""
        return "周" + arr[n - 1]
    }

    // 日期格式化: "2026-08-17" → "08-17"(去掉年份)
    function shortDate(dateStr) {
        if (!dateStr) return ""
        return dateStr.length >= 10 ? dateStr.substring(5) : dateStr
    }

    // 组合 Tips 文本(对齐 widget.cpp 的 QString tips = "天气 ... 湿度X%  风向X X级")
    function tipsString() {
        if (!liveData.weather) return ""
        var s = "天气 " + liveData.weather
        if (liveData.humidity) s += "  湿度" + liveData.humidity + "%"
        if (liveData.windDirection)
            s += "  风向" + liveData.windDirection + " " + liveData.windPower + "级"
        return s
    }

    // ---- 信号绑定 ----
    Connections {
        target: weather
        onLocationsReady: {
            root.cityName = city
            weather.queryCurrentWeather(adcode)
            weather.queryForecast(adcode)
        }
        onCurrentWeatherReady: {
            root.liveData = live
        }
        onForecastReady: {
            root.forecastData = forecast
        }
        onErrorOccurred: {
            console.log("天气错误:", message)
        }
    }

    // 10 分钟定时刷新(对应原 Widget 的 refreshTimer)
    Timer {
        interval: 10 * 60 * 1000
        repeat: true
        running: true
        onTriggered: weather.queryLocation()
    }

    // ---- 背景:beijing.png 拉伸填充(对齐原版 Widget styleSheet border-image)----
    Image {
        anchors.fill: parent
        source: "qrc:/beijing.png"
        fillMode: Image.Stretch
    }

    // ---- 顶部信息栏(单行横排,精确复刻 widget.ui 第 42-223 行)----
    Row {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 5
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        height: 124
        spacing: 0

        // 左侧:location(96x96) + city(34pt 居中, min 170w)
        Row {
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter
            Image {
                source: "qrc:/weather/local.png"
                width: 96; height: 96
                fillMode: Image.PreserveAspectFit
                anchors.verticalCenter: parent.verticalCenter
            }
            Item {
                width: 170; height: 96
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    anchors.fill: parent
                    text: root.cityName || "定位中..."
                    color: "white"
                    font.pixelSize: 34
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // 中间:temperature(Noto Mono 68pt 居中) + sheshidu(28pt 顶部对齐)
        Row {
            spacing: 0
            anchors.verticalCenter: parent.verticalCenter
            Text {
                text: root.liveData.temperature || "--"
                color: "white"
                font.family: "Noto Mono"
                font.pixelSize: 68
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                width: 120
            }
            // sheshidu:独立小 label,AlignHCenter|AlignTop
            Item {
                width: 40; height: 96
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "℃"
                    color: "white"
                    font.pixelSize: 28
                    width: 40
                }
            }
        }

        // 弹性 spacer(让右侧内容推到最右)
        Item {
            width: Math.max(20, parent.width - 96 - 6 - 170 - 120 - 40 - 60 - 40 - 260)
            height: 1
        }

        // 右侧:两行(对齐 widget.ui verticalLayout:horizontalLayout_5 + tipsText)
        Column {
            spacing: 0
            anchors.verticalCenter: parent.verticalCenter
            // 第一行:spacer(40px) + Tips(60x38 固定)
            Row {
                spacing: 0
                Item { width: 40; height: 38 }   // horizontalSpacer_9
                Text {
                    text: "Tips"
                    color: "white"
                    font.pixelSize: 20
                    width: 60; height: 38
                    verticalAlignment: Text.AlignVCenter
                }
            }
            // 第二行:tipsText(右对齐,固定高 48, min 100x48)
            Text {
                text: root.tipsString()
                color: "white"
                font.pixelSize: 20
                height: 48
                width: 260
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // ---- 卡片区:3 卡默认可见 + 横向 Flickable 看第 4 张 ----
    Flickable {
        id: cardsFlickable
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        // 内容宽:左边距 50 + 4 张卡(290) + 3 个间距(40) = 1330
        contentWidth: 50 + 4 * 290 + 3 * 40
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        // 不设 snapMode,松手停哪算哪
        clip: true

        // 4 张卡绝对定位(复刻原版 column1/2/3 坐标 + 新增第 4 张)
        // 第 1 张:今天  x=50
        Item {
            x: 50; y: 30; width: 290; height: 420
            visible: root.forecastData.days && root.forecastData.days.length >= 1
            WeatherCard {
                anchors.fill: parent
                dayData: root.forecastData.days ? root.forecastData.days[0] : ({})
                weekLabel: "今天"
                dateLabel: root.forecastData.days
                            ? root.shortDate(root.forecastData.days[0].date) : ""
                iconNumber: root.forecastData.days
                            ? root.weatherIconNumber(root.forecastData.days[0].dayWeather) : "3"
            }
        }
        // 第 2 张:明天  x=380 (50+290+40)
        Item {
            x: 380; y: 30; width: 290; height: 420
            visible: root.forecastData.days && root.forecastData.days.length >= 2
            WeatherCard {
                anchors.fill: parent
                dayData: root.forecastData.days ? root.forecastData.days[1] : ({})
                weekLabel: "明天"
                dateLabel: root.forecastData.days
                            ? root.shortDate(root.forecastData.days[1].date) : ""
                iconNumber: root.forecastData.days
                            ? root.weatherIconNumber(root.forecastData.days[1].dayWeather) : "3"
            }
        }
        // 第 3 张:后天  x=710 (380+290+40)
        Item {
            x: 710; y: 30; width: 290; height: 420
            visible: root.forecastData.days && root.forecastData.days.length >= 3
            WeatherCard {
                anchors.fill: parent
                dayData: root.forecastData.days ? root.forecastData.days[2] : ({})
                weekLabel: "后天"
                dateLabel: root.forecastData.days
                            ? root.shortDate(root.forecastData.days[2].date) : ""
                iconNumber: root.forecastData.days
                            ? root.weatherIconNumber(root.forecastData.days[2].dayWeather) : "3"
            }
        }
        // 第 4 张:大后天 x=1040 (710+290+40,完全在屏幕外,需横向滑动)
        Item {
            x: 1040; y: 30; width: 290; height: 420
            visible: root.forecastData.days && root.forecastData.days.length >= 4
            WeatherCard {
                anchors.fill: parent
                dayData: root.forecastData.days ? root.forecastData.days[3] : ({})
                weekLabel: root.forecastData.days
                            ? root.weekText(root.forecastData.days[3].week) : ""
                dateLabel: root.forecastData.days
                            ? root.shortDate(root.forecastData.days[3].date) : ""
                iconNumber: root.forecastData.days
                            ? root.weatherIconNumber(root.forecastData.days[3].dayWeather) : "3"
            }
        }
    }

    // ---- 启动时自动查询(对应原 Widget 构造函数末尾的 weather->queryLocation())----
    Component.onCompleted: {
        weather.queryLocation()
    }
}
