#ifndef WEATHER_H
#define WEATHER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QVariantMap>
#include <QVariantList>
#include <QNetworkAccessManager>

// 高德服务:
// 定位:           https://restapi.amap.com/v3/ip?key=cc52c6816bad728950f183347978ccff
// 天气预报(4天):  https://restapi.amap.com/v3/weather/weatherInfo?city=350100&key=...&extensions=all
// 当前天气:        https://restapi.amap.com/v3/weather/weatherInfo?city=350100&key=...

class Weather : public QObject
{
    Q_OBJECT
public:
    // 内部 struct 保留(C++ 端使用,QML 不直接访问)
    struct LiveWeather {
        QString province, city, adcode;
        QString weather, temperature;
        QString windDirection, windPower;
        QString humidity, reportTime;
    };
    struct ForecastDay {
        QString date, week;
        QString dayWeather, nightWeather;
        QString dayTemp, nightTemp;
        QString dayWind, nightWind;
    };
    struct Forecast {
        QString city, adcode, reportTime;
        QVector<ForecastDay> days;   // 当天 + 未来3天 = 4天
    };

    explicit Weather(QObject *parent = nullptr);

    // QML 可主动调用(Q_INVOKABLE)
    Q_INVOKABLE void queryLocation();
    Q_INVOKABLE void queryCurrentWeather(const QString &adcode);
    Q_INVOKABLE void queryForecast(const QString &adcode);

Q_SIGNALS:
    // 信号参数全部改为 QVariantMap / QVariantList,QML 端可直接访问字段
    void locationsReady(const QString &city, const QString &adcode);
    void currentWeatherReady(const QVariantMap &live);
    void forecastReady(const QVariantMap &forecast);   // 含 city/reportTime/days
    void errorOccurred(const QString &message);

private:
    QNetworkAccessManager *netManager;
    const QString apiKey;
    void parseLocation(QNetworkReply *reply);
    void parseCurrent(QNetworkReply *reply);
    void parseForecast(QNetworkReply *reply);

    // struct → QVariantMap 转换辅助函数
    static QVariantMap liveToMap(const LiveWeather &live);
    static QVariantMap dayToMap(const ForecastDay &day);
};

#endif // WEATHER_H
