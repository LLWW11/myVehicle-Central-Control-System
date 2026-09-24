#include "Mpg123Runtime.h"
#include <mpg123.h>

Mpg123Runtime::Mpg123Runtime()
{
    const int res = mpg123_init();
    if (res == MPG123_OK)
    {
        m_initialized = true;
        return;
    }
    else
    {
        m_errorCode = res;
        m_errorString = QString::fromLocal8Bit(mpg123_plain_strerror(res));
    }
}

Mpg123Runtime::~Mpg123Runtime()
{
    if (m_initialized)
        mpg123_exit();
}

bool Mpg123Runtime::isValid() const
{
    return m_initialized;
}
const QString &Mpg123Runtime::errorString() const
{
    return m_errorString;
}
int Mpg123Runtime::errorCode() const
{
    return m_errorCode;
}
