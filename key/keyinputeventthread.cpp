/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @projectName   key
* @brief         板载 KEY0 输入监听线程实现
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @link          www.openedv.com
* @date          2024-11-27
*******************************************************************/
#include "keyinputeventthread.h"

#include <QDebug>
#include <QKeySequence>

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

KeyInputEventThread::KeyInputEventThread(QObject *parent)
    : QThread(parent)
{
    // SystemUI 创建该对象后立即开始监听，避免依赖界面焦点。
    start();
}

KeyInputEventThread::~KeyInputEventThread()
{
    // poll 使用短超时，因此线程可以在退出请求后及时结束。
    requestInterruption();
    wait(1000);
}

int KeyInputEventThread::openKeyInputDevice() const
{
    DIR *inputDirectory = opendir("/dev/input");
    if (inputDirectory == nullptr) {
        qWarning() << "无法打开 /dev/input:" << std::strerror(errno);
        return -1;
    }

    int keyFileDescriptor = -1;
    while (dirent *entry = readdir(inputDirectory)) {
        if (std::strncmp(entry->d_name, "event", 5) != 0)
            continue;

        const int candidate = openat(dirfd(inputDirectory), entry->d_name,
                                     O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (candidate < 0)
            continue;

        char deviceName[80] = {};
        if (ioctl(candidate, EVIOCGNAME(sizeof(deviceName) - 1), deviceName) < 1)
            deviceName[0] = '\0';

        // 正点原子不同内核版本中设备名可能使用下划线或连字符。
        if (std::strstr(deviceName, "gpio_keys") != nullptr
                || std::strstr(deviceName, "gpio-keys") != nullptr) {
            keyFileDescriptor = candidate;
            break;
        }

        close(candidate);
    }

    closedir(inputDirectory);
    return keyFileDescriptor;
}

void KeyInputEventThread::run()
{
    const int keyFileDescriptor = openKeyInputDevice();
    if (keyFileDescriptor < 0) {
        qWarning() << "未找到板载 gpio-keys 输入设备";
        return;
    }

    pollfd keyPollDescriptor = {};
    keyPollDescriptor.fd = keyFileDescriptor;
    keyPollDescriptor.events = POLLIN;

    while (!isInterruptionRequested()) {
        const int pollResult = poll(&keyPollDescriptor, 1, 200);
        if (pollResult <= 0 || !(keyPollDescriptor.revents & POLLIN))
            continue;

        input_event event = {};
        if (read(keyFileDescriptor, &event, sizeof(event)) != sizeof(event))
            continue;

        // 原 KEY0 测试程序将板载按键映射为 Linux KEY_VOLUMEDOWN。
        if (event.type == EV_KEY && event.code == KEY_VOLUMEDOWN)
            Q_EMIT keyEvent(Qt::Key_VolumeDown, event.value != 0);
    }

    close(keyFileDescriptor);
}
