#ifndef SERIALLOCATION_H
#define SERIALLOCATION_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
// 接受数据并解析
class SerialLocation : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool valid READ valid NOTIFY locationChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY locationChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY locationChanged)
    Q_PROPERTY(double accuracy READ accuracy NOTIFY locationChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    // explicit SerialLocation(QObject *parent = nullptr);
    explicit SerialLocation(QObject *parent = nullptr);
    bool openPort(const QString &deviceName, qint32 baudRate);
    bool valid() const; // 数据是否有效
    double latitude() const;
    double longitude() const;
    double accuracy() const;
    QString statusText() const; // 串口状态

signals:
    void locationChanged();
    void statusChanged();

private slots:
    void readAvailableData();
    void handleSerialError(QSerialPort::SerialPortError error);
    void handleLocationTimeout();

private:
    bool parseFrame(const QByteArray &frame,
                    double &latitude,
                    double &longitude,
                    double &accuracy) const; // 解析成功返回 true
    QSerialPort m_serialPort;                // 串口类
    QByteArray m_receiveBuffer;              // 接受到的数据
    QTimer m_locationTimeoutTimer;           // 超时定时器
    bool m_valid = false;
    double m_latitude = 0.0;
    double m_longitude = 0.0;
    double m_accuracy = 0.0;
    QString m_statusText = QStringLiteral("串口尚未打开");
};

#endif