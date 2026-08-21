import QtQuick 2.12

Item {
    property string system_time: Qt.formatDateTime(new Date(), "hh:mm" )
    property string system_week: "周三"
    property string system_date1: ""
    property string system_date2: ""
    property string system_date3: "10"

    function currentTime(){
        return Qt.formatDateTime(new Date(), "hh:mm" )
    }

    function currentDate1(){
        return Qt.formatDateTime(new Date(), "ddd MM,dd" )
    }

    function currentWeek(){
        return Qt.formatDateTime(new Date(), "ddd" )
    }

    function currentDate2(){
        return Qt.formatDateTime(new Date(), "MM月dd日" )
    }

    function currentDate3(){
        return Qt.formatDateTime(new Date(), "dd" )
    }


    Timer{
        id: timer
        interval: 1000
        repeat: true
        running: parent.enabled
        onTriggered:{
            system_time = currentTime()
            system_date1 = currentDate1()
            system_date2 = currentDate2()
            system_date3 = currentDate3()
            system_week = currentWeek()

            if (system_week === "Sun")
                system_week = "周日"
            else if (system_week === "Mon")
                system_week = "周一"
            else if (system_week === "Tue")
                system_week = "周二"
            else if (system_week === "Wed")
                system_week = "周三"
            else if (system_week === "Thu")
                system_week = "周四"
            else if (system_week === "Fri")
                system_week = "周五"
            else if (system_week === "Sat")
                system_week = "周六"
        }
    }
}
