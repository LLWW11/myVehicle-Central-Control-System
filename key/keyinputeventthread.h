/******************************************************************
Copyright © Deng Zhimao Co., Ltd. 2021-2030. All rights reserved.
* @projectName   key
* @brief         板载 KEY0 输入监听线程
* @author        Deng Zhimao
* @email         dengzhimao@alientek.com/1252699831@qq.com
* @link          www.openedv.com
* @date          2024-11-27
*******************************************************************/
#ifndef KEYINPUTEVENTTHREAD_H
#define KEYINPUTEVENTTHREAD_H

#include <QThread>

/**
 * @brief 在独立线程中监听 Linux gpio-keys 输入设备。
 *
 * SystemUI 常驻运行时使用该类监听板载 KEY0，因此应用窗口位于前台时
 * 仍能收到按键事件。
 */
class KeyInputEventThread : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief 创建按键监听线程并立即开始监听。
     * @param parent Qt 对象父节点。
     */
    explicit KeyInputEventThread(QObject *parent = nullptr);

    /**
     * @brief 请求监听线程停止并释放输入设备。
     */
    ~KeyInputEventThread() override;

Q_SIGNALS:
    /**
     * @brief 上报板载按键状态变化。
     * @param code Qt 按键码。
     * @param pressed true 表示按下，false 表示释放。
     */
    void keyEvent(int code, bool pressed);

protected:
    /**
     * @brief 查找 gpio-keys 设备并持续读取 KEY0 事件。
     */
    void run() override;

private:
    /**
     * @brief 查找并打开板载 gpio-keys 输入设备。
     * @return 成功时返回文件描述符，失败时返回 -1。
     */
    int openKeyInputDevice() const;
};

#endif // KEYINPUTEVENTTHREAD_H
