#include "weather.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextCodec>
#include <QDebug>
#include <QString>
#include <QHostInfo>
#include <QHostAddress>
#include <QAbstractSocket>
#include <QSslConfiguration>
#include <QSslSocket>

Weather::Weather(QObject *parent)
    : QObject(parent)
    , netManager(new QNetworkAccessManager(this))
    , apiKey(QStringLiteral("<your Key>"))
{
}

// https://restapi.amap.com/v3/ip?key=<>
void Weather::queryLocation()
{
    // 自己解析域名并强制直连 IPv4:系统优先 IPv6 时高德按源 IP 定位会返回空
    QHostInfo::lookupHost(QStringLiteral("restapi.amap.com"), this,
        [this](const QHostInfo &info) {

            for (const QHostAddress &addr : info.addresses()) {
                if (addr.protocol() != QAbstractSocket::IPv4Protocol)
                    continue;                     // 跳过 IPv6 记录

                QUrl url(QStringLiteral("https://%1/v3/ip").arg(addr.toString()));
                QUrlQuery myquery;
                myquery.addQueryItem("key", apiKey);
                url.setQuery(myquery);

                QNetworkRequest req(url);
                req.setRawHeader("Host", "restapi.amap.com");   // 服务器按 Host 头路由
                QSslConfiguration ssl;
                ssl.setPeerVerifyMode(QSslSocket::VerifyNone);  // IP 直连不校验证书
                req.setSslConfiguration(ssl);

                QNetworkReply *reply = netManager->get(req);
                connect(reply,
                        &QNetworkReply::finished,
                        this,
                        [this, reply]() { parseLocation(reply); });
                return;
            }

            emit errorOccurred(QStringLiteral("无法解析高德的IPv4地址"));
        });
}

// https://restapi.amap.com/v3/weather/weatherInfo?city=350100&key=<>
void Weather::queryCurrentWeather(const QString &adcode)
{
    QUrl url(QStringLiteral("https://restapi.amap.com/v3/weather/weatherInfo"));
    QUrlQuery myquery;
    myquery.addQueryItem(QString("city"), adcode);
    myquery.addQueryItem(QString("key"), apiKey);

    url.setQuery(myquery);
    QNetworkReply *reply = netManager->get(QNetworkRequest(url));
    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply]() { parseCurrent(reply); });
}

// https://restapi.amap.com/v3/weather/weatherInfo?city=350100&key=<>
void Weather::queryForecast(const QString &adcode)
{
    QUrl url(QStringLiteral("https://restapi.amap.com/v3/weather/weatherInfo"));
    QUrlQuery myquery;
    myquery.addQueryItem(QString("key"), apiKey);
    myquery.addQueryItem(QString("city"), adcode);
    myquery.addQueryItem(QString("extensions"), QString("all"));

    url.setQuery(myquery);
    QNetworkReply *reply = netManager->get(QNetworkRequest(url));
    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply]() { parseForecast(reply); });
}

void Weather::parseCurrent(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    qDebug() << data;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        emit errorOccurred(QStringLiteral("当前天气解析失败"));
        return;
    }
    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != QStringLiteral("1")) {
        emit errorOccurred(obj.value("info").toString());
        return;
    }

    QJsonArray lives = obj.value("lives").toArray();
    if (lives.isEmpty()) {
        emit errorOccurred(QStringLiteral("当前天气数据为空"));
        return;
    }

    QJsonObject liveObj = lives.at(0).toObject();
    LiveWeather live;
    live.province      = liveObj.value("province").toString();
    live.city          = liveObj.value("city").toString();
    live.adcode        = liveObj.value("adcode").toString();
    live.weather       = liveObj.value("weather").toString();
    live.temperature   = liveObj.value("temperature").toString();
    live.windDirection = liveObj.value("winddirection").toString();
    live.windPower     = liveObj.value("windpower").toString();
    live.humidity      = liveObj.value("humidity").toString();
    live.reportTime    = liveObj.value("reporttime").toString();
    // 改造点:struct → QVariantMap 后再 emit,QML 端可直接访问字段
    emit currentWeatherReady(liveToMap(live));

    reply->deleteLater();
}

void Weather::parseLocation(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        emit errorOccurred(QStringLiteral("定位响应解析失败"));
        return;
    }
    QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QStringLiteral("1")) {
        emit errorOccurred(obj.value(QStringLiteral("info")).toString());  // 高德错误信息
        return;
    }
    emit locationsReady(obj.value("city").toString(),
                        obj.value("adcode").toString());
}

void Weather::parseForecast(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        emit errorOccurred(QStringLiteral("天气预报解析失败"));
        return;
    }
    QJsonObject obj = doc.object();
    if (obj.value("status").toString() != QStringLiteral("1")) {
        emit errorOccurred(obj.value("info").toString());
        return;
    }

    QJsonArray forecasts = obj.value("forecasts").toArray();
    if (forecasts.isEmpty()) {
        emit errorOccurred(QStringLiteral("天气预报数据为空"));
        return;
    }

    QJsonObject fcObj = forecasts.at(0).toObject();
    Forecast forecast;
    forecast.city       = fcObj.value("city").toString();
    forecast.adcode     = fcObj.value("adcode").toString();
    forecast.reportTime = fcObj.value("reporttime").toString();

    // 原样保留:循环遍历全部 casts,4 天数据全部 append
    QJsonArray casts = fcObj.value("casts").toArray();
    for (const QJsonValue &value : casts) {
        QJsonObject castObj = value.toObject();
        ForecastDay day;
        day.date         = castObj.value("date").toString();
        day.week         = castObj.value("week").toString();
        day.dayWeather   = castObj.value("dayweather").toString();
        day.nightWeather = castObj.value("nightweather").toString();
        day.dayTemp      = castObj.value("daytemp").toString();
        day.nightTemp    = castObj.value("nighttemp").toString();
        day.dayWind      = castObj.value("daywind").toString();
        day.nightWind    = castObj.value("nightwind").toString();
        forecast.days.append(day);
    }

    // 改造点:struct → QVariantMap 后再 emit,QML 端 forecast.days 是数组
    QVariantMap map;
    map.insert("city", forecast.city);
    map.insert("adcode", forecast.adcode);
    map.insert("reportTime", forecast.reportTime);
    QVariantList dayList;
    for (const ForecastDay &d : forecast.days)
        dayList.append(dayToMap(d));
    map.insert("days", dayList);
    emit forecastReady(map);
}

// struct → QVariantMap 辅助函数
QVariantMap Weather::liveToMap(const LiveWeather &live)
{
    QVariantMap m;
    m.insert("province",      live.province);
    m.insert("city",          live.city);
    m.insert("adcode",        live.adcode);
    m.insert("weather",       live.weather);
    m.insert("temperature",   live.temperature);
    m.insert("windDirection", live.windDirection);
    m.insert("windPower",     live.windPower);
    m.insert("humidity",      live.humidity);
    m.insert("reportTime",    live.reportTime);
    return m;
}

QVariantMap Weather::dayToMap(const ForecastDay &day)
{
    QVariantMap m;
    m.insert("date",         day.date);
    m.insert("week",         day.week);
    m.insert("dayWeather",   day.dayWeather);
    m.insert("nightWeather", day.nightWeather);
    m.insert("dayTemp",      day.dayTemp);
    m.insert("nightTemp",    day.nightTemp);
    m.insert("dayWind",      day.dayWind);
    m.insert("nightWind",    day.nightWind);
    return m;
}
