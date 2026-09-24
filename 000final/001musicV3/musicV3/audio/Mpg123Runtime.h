#ifndef MPG123RUNTIME_H
#define MPG123RUNTIME_H

#include <QString>
#include <QtGlobal>

// 一个音乐应用只能实例化一个这个对象
class Mpg123Runtime final // 不能被继承
{
public:
    Mpg123Runtime();
    ~Mpg123Runtime();
    Q_DISABLE_COPY(Mpg123Runtime);
    bool isValid() const;
    const QString &errorString() const;
    int errorCode() const;

private:
    bool m_initialized = false;
    int m_errorCode = 0;
    QString m_errorString;
};

#endif /* MPG123RUNTIME_H */
