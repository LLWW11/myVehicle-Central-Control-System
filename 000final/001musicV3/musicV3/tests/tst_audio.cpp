#include "audio/AudioFormat.h"
#include "MusicCache.h"
#include "MusicListModel.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QtTest>

/** @brief 不依赖真实声卡的格式、缓存身份和列表边界测试。 */
class AudioTests final : public QObject
{
    Q_OBJECT
private slots:
    /** @brief 使测试缓存与用户正常缓存隔离。 */
    void initTestCase();
    /** @brief 验证单声道和双声道 S16 的每帧字节数。 */
    void formatFrameBytes();
    /** @brief 验证音源、歌曲 ID 和音质均参与缓存身份。 */
    void cacheIdentity();
    /** @brief 验证空列表、首尾循环和非法索引处理。 */
    void listNavigation();
};

/** @brief 启用 Qt 专用测试路径。 */
void AudioTests::initTestCase()
{
    QCoreApplication::setApplicationName(QStringLiteral("musicV3-tests"));
    QStandardPaths::setTestModeEnabled(true);
}

/** @brief S16 单声道一帧两字节，双声道一帧四字节。 */
void AudioTests::formatFrameBytes()
{
    AudioFormat format;
    QVERIFY(!format.isValid());
    QCOMPARE(format.bytesPerFrame(), 0);
    format.sampleRate = 44100;
    format.channels = 1;
    QVERIFY(format.isValid());
    QCOMPARE(format.bytesPerFrame(), 2);
    format.channels = 2;
    QCOMPARE(format.bytesPerFrame(), 4);
    AudioFormat same = format;
    QVERIFY(same == format);
    same.sampleRate = 48000;
    QVERIFY(same != format);
}

/** @brief 不同歌曲身份不会复用同一个 MP3 缓存文件。 */
void AudioTests::cacheIdentity()
{
    const QString a = MusicCache::mp3File(
        QStringLiteral("wy"), QStringLiteral("1"), QStringLiteral("320k"));
    const QString b = MusicCache::mp3File(
        QStringLiteral("wy"), QStringLiteral("1"), QStringLiteral("128k"));
    const QString c = MusicCache::mp3File(
        QStringLiteral("tx"), QStringLiteral("1"), QStringLiteral("320k"));
    QVERIFY(!a.isEmpty());
    QVERIFY(a != b);
    QVERIFY(a != c);
}

/** @brief 索引只在合法范围内变化，上一首和下一首循环。 */
void AudioTests::listNavigation()
{
    MusicListModel model;
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.currentIndex(), -1);
    QCOMPARE(model.nextIndex(), -1);
    model.addPresets();
    QVERIFY(model.count() > 1);
    QCOMPARE(model.currentIndex(), 0);
    QCOMPARE(model.previousIndex(), model.count() - 1);
    model.setCurrentIndex(model.count() - 1);
    QCOMPARE(model.nextIndex(), 0);
    model.setCurrentIndex(model.count());
    QCOMPARE(model.currentIndex(), model.count() - 1);
}

QTEST_APPLESS_MAIN(AudioTests)
#include "tst_audio.moc"
