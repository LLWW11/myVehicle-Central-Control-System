#ifndef LYRICPARSER_H
#define LYRICPARSER_H

#include <QString>
#include <QList>
#include <QRegularExpression>

// LRC 歌词中的单行
struct LyricLine {
    qint64  time;   // 毫秒
    QString text;
};
Q_DECLARE_METATYPE(LyricLine)
// 轻量级 LRC 歌词解析器
// 支持标准格式:
//   [mm:ss.xx]text
//   [mm:ss]text
// 多时间戳合并为多行（如 [00:01.00][00:05.00]text → 两条 LyricLine）
// 解析后按时间升序排列
namespace LyricParser {

inline QList<LyricLine> parseLrc(const QString& lrc)
{
    QList<LyricLine> lines;
    if (lrc.isEmpty()) return lines;

    // 匹配行首的 [mm:ss.xx] 或 [mm:ss.xxx] 或 [mm:ss]
    static QRegularExpression timeTag("\\[(\\d{2,3}):(\\d{2})(?:\\.(\\d{2,3}))?\\]");

    const QStringList rawLines = lrc.split('\n', QString::SkipEmptyParts);

    for (const QString& raw : rawLines) {
        QString trimmed = raw.trimmed();
        if (trimmed.isEmpty()) continue;

        // 收集该行所有时间戳
        struct Tag { qint64 ms; int endPos; };
        QList<Tag> tags;

        QRegularExpressionMatchIterator it = timeTag.globalMatch(trimmed);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            int minutes  = m.captured(1).toInt();
            int seconds  = m.captured(2).toInt();
            QString frac = m.captured(3);
            int millis   = frac.isEmpty() ? 0
                         : (frac.length() == 2 ? frac.toInt() * 10 : frac.toInt());
            qint64 ms = static_cast<qint64>(minutes) * 60000
                      + static_cast<qint64>(seconds) * 1000
                      + millis;
            tags.append({ms, m.capturedEnd()});
        }

        if (tags.isEmpty()) continue;

        // 最后一个时间戳之后的内容即为歌词文本
        QString text = trimmed.mid(tags.last().endPos).trimmed();
        // 过滤掉元信息行（offset / title / artist 等）
        if (text.isEmpty() && tags.size() == 1) continue;

        if (text.isEmpty()) {
            // 纯时间戳无文本行（可能是间奏标记），保留但文本为空
            text = QStringLiteral("♪");
        }

        for (const Tag& t : tags) {
            LyricLine line;
            line.time = t.ms;
            line.text = text;
            lines.append(line);
        }
    }

    // 按时间升序
    std::sort(lines.begin(), lines.end(),
              [](const LyricLine& a, const LyricLine& b) {
                  return a.time < b.time;
              });
    return lines;
}

} // namespace LyricParser

#endif // LYRICPARSER_H
