
import QtQuick 2.12

Item {
    id: card
    // 输入属性
    property var dayData: ({})            // QVariantMap(单天数据)
    property string weekLabel: ""          // "今天" / "明天" / "后天" / "周X"
    property string dateLabel: ""          // "08-17"
    property string iconNumber: "3"        // 1-12.png 图标号

    // 不加卡片背景(对齐原版 widget.ui 未设置 QSS 背景的策略)

    Column {
        anchors.fill: parent
        spacing: 0

        // 日期
        Item {
            width: parent.width
            height: 68
            Text {
                anchors.fill: parent
                text: card.weekLabel
                color: "white"
                font.pixelSize: 34
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        // 图标
        Item {
            width: parent.width
            height: 235
            Row {
                anchors.fill: parent
                spacing: 0
                Item { width: 27; height: 235 }   // 左 spacer
                Image {
                    width: 235; height: 235
                    source: "qrc:/weather/" + card.iconNumber + ".png"
                    fillMode: Image.PreserveAspectFit
                }
                Item { width: 27; height: 235 }   // 右 spacer
            }
        }

        // 天气文字
        Item {
            width: parent.width
            height: 50
            Text {
                anchors.fill: parent
                text: card.dayData.dayWeather || ""
                color: "white"
                font.pixelSize: 25
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        // 温度范围
        Item {
            width: parent.width
            height: 50
            Text {
                anchors.fill: parent
                text: (card.dayData.dayTemp && card.dayData.nightTemp)
                      ? card.dayData.dayTemp + "～" + card.dayData.nightTemp + "℃"
                      : ""
                color: "white"
                font.pixelSize: 25
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
