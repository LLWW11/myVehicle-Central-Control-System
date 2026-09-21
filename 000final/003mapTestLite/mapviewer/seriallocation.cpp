#include "seriallocation.h"
#include <QtMath>

SerialLocation::SerialLocation(QObject *parent)
    : QObject(parent)
{
    connect(&m_serialPort, &QSerialPort::readyRead,
            this, &SerialLocation::readAvailableData);
    connect(&m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialLocation::handleSerialError);

    m_locationTimeoutTimer.setSingleShot(true);
    m_locationTimeoutTimer.setInterval(5000);
    connect(&m_locationTimeoutTimer, &QTimer::timeout,
            this, &SerialLocation::handleLocationTimeout);
}

bool SerialLocation::openPort(const QString &deviceName, qint32 baudRate)
{
    if (m_serialPort.isOpen())
        m_serialPort.close();

    m_serialPort.setPortName(deviceName);

    if (!m_serialPort.open(QIODevice::ReadWrite))
    {
        m_statusText = QStringLiteral("串口打开失败：") + m_serialPort.errorString();
        Q_EMIT statusChanged();
        return false;
    }

    const bool configured =
        m_serialPort.setBaudRate(baudRate) && m_serialPort.setDataBits(QSerialPort::Data8) && m_serialPort.setParity(QSerialPort::NoParity) && m_serialPort.setStopBits(QSerialPort::OneStop) && m_serialPort.setFlowControl(QSerialPort::NoFlowControl);

    if (!configured)
    {
        m_statusText = QStringLiteral("串口参数配置失败：") + m_serialPort.errorString();
        m_serialPort.close();
        Q_EMIT statusChanged();
        return false;
    }
    m_valid = false;
    m_statusText = QStringLiteral("串口已连接，等待定位数据");
    m_locationTimeoutTimer.start(); // 开始等待数据

    Q_EMIT locationChanged();
    Q_EMIT statusChanged();

    return true;
}

bool SerialLocation::valid() const
{
    return m_valid;
}
double SerialLocation::latitude() const
{
    return m_latitude;
}
double SerialLocation::longitude() const
{
    return m_longitude;
}
double SerialLocation::accuracy() const
{
    return m_accuracy;
}

QString SerialLocation::statusText() const
{
    return m_statusText;
}

void SerialLocation::readAvailableData()
{
    m_receiveBuffer.append(m_serialPort.readAll());

    int newlineIndex = -1;

    while ((newlineIndex = m_receiveBuffer.indexOf('\n')) >= 0)
    {
        const QByteArray frame =
            m_receiveBuffer.left(newlineIndex).trimmed();

        m_receiveBuffer.remove(0, newlineIndex + 1);

        double latitude = 0.0;
        double longitude = 0.0;
        double accuracy = 0.0;

        if (!parseFrame(frame, latitude, longitude, accuracy))
            continue;

        m_latitude = latitude;
        m_longitude = longitude;
        m_accuracy = accuracy;
        m_valid = true;
        m_statusText = QStringLiteral("定位数据正常");
        m_locationTimeoutTimer.start(); // 数据正常则开启定时
        Q_EMIT locationChanged();
        Q_EMIT statusChanged();
    }

    // 防止一直收不到换行符时缓存无限增长。
    if (m_receiveBuffer.size() > 4096)
    {
        m_receiveBuffer.clear();
        m_statusText = QStringLiteral("串口数据格式异常");
        Q_EMIT statusChanged();
    }
}
void SerialLocation::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;
    m_locationTimeoutTimer.stop();
    m_statusText = QStringLiteral("串口错误：") + m_serialPort.errorString();

    Q_EMIT statusChanged();
}
void SerialLocation::handleLocationTimeout()
{
    m_valid = false;
    m_statusText = QStringLiteral("异常:5秒未收到有效数据");

    Q_EMIT locationChanged();
    Q_EMIT statusChanged();
}

// 解析
bool SerialLocation::parseFrame(const QByteArray &frame,
                                double &latitude,
                                double &longitude,
                                double &accuracy) const
{
    const QList<QByteArray> fields = frame.split(',');

    if (fields.size() != 4 || fields.at(0) != "$LOC")
        return false;

    bool latitudeOk = false;
    bool longitudeOk = false;
    bool accuracyOk = false;

    latitude = fields.at(1).toDouble(&latitudeOk);
    longitude = fields.at(2).toDouble(&longitudeOk);
    accuracy = fields.at(3).toDouble(&accuracyOk);

    return latitudeOk && longitudeOk && accuracyOk && qIsFinite(latitude) && qIsFinite(longitude) && qIsFinite(accuracy) && latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0 && accuracy >= 0.0;
}