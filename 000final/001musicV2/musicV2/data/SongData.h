#ifndef SONGDATA_H
#define SONGDATA_H

#include "Song.h"
#include <QList>

// 预置歌曲列表(与 000CMu14 原工程一致)
// songId 来自网易云公开接口 (music.163.com/api/song/detail)
// 播放 URL 由音源 API (source.shiqianjiang.cn) 动态解析
// 封面 URL 来自网易 CDN (p1/p2.music.126.net)
inline QList<Song> presetSongs()
{
    return {
        {"28815250", QStringLiteral("平凡之路"), QStringLiteral("朴树"), QStringLiteral("猎户星座"),
         "wy", 302, "https://p2.music.126.net/W_5XiCv3rGS1-J7EXpHSCQ==/18885211718782327.jpg", "320k"},

        {"254574", QStringLiteral("后来"), QStringLiteral("刘若英"), QStringLiteral("我等你"),
         "wy", 341, "https://p2.music.126.net/eBF7bHnJYBUfOFrJ_7SUfw==/109951163351825356.jpg", "320k"},

        {"347230", QStringLiteral("海阔天空"), QStringLiteral("Beyond"), QStringLiteral("海阔天空"),
         "wy", 326, "https://p2.music.126.net/iAwVf8ag_45csIUuh1wSZg==/109951168912558470.jpg", "320k"},

        {"436514312", QStringLiteral("成都"), QStringLiteral("赵雷"), QStringLiteral("成都"),
         "wy", 328, "https://p2.music.126.net/34YW1QtKxJ_3YnX9ZzKhzw==/2946691234868155.jpg", "320k"},

        {"5308009", QStringLiteral("Prelude"), QStringLiteral("July"), QStringLiteral("July"),
         "wy", 98, "https://p1.music.126.net/vCJtBU6hbgrydCorFbOMGA==/109951163240198188.jpg", "320k"},

        {"1824020871", QStringLiteral("One Last Kiss"), QStringLiteral("\u5b87\u591a\u7530\u30d2\u30ab\u30eb"), QStringLiteral("One Last Kiss"),
         "wy", 252, "https://p2.music.126.net/l3G4LigZnOxFE9lB4bz_LQ==/109951165791860501.jpg", "320k"},

        {"5257138", QStringLiteral("屋顶"), QStringLiteral("周杰伦、温岚、吴宗宪"), QStringLiteral("男女情歌对唱冠军全记录"),
         "wy", 319, "https://p1.music.126.net/81BsxxhomJ4aJZYvEbyPkw==/109951165671182684.jpg", "320k"},

        {"210049", QStringLiteral("布拉格广场"), QStringLiteral("蔡依林、周杰伦"), QStringLiteral("看我72变"),
         "wy", 294, "https://p1.music.126.net/8D2Pd7EuvGboMyE2xWc47A==/109951172453712025.jpg", "320k"},

        {"255020", QStringLiteral("刀马旦"), QStringLiteral("李玛、周杰伦"), QStringLiteral("Promise"),
         "wy", 192, "https://p1.music.126.net/dDgHDWlJAFwkMNrjbQExIA==/109951165959446596.jpg", "320k"},

        {"65800", QStringLiteral("最佳损友"), QStringLiteral("陈奕迅"), QStringLiteral("Life Continues..."),
         "wy", 233, "https://p1.music.126.net/3mi073axgjg-g-79ObwwEQ==/109951171836582062.jpg", "320k"},

        {"65766", QStringLiteral("富士山下"), QStringLiteral("陈奕迅"), QStringLiteral("What's Going On...?"),
         "wy", 258, "https://p1.music.126.net/oSMs7RzJFx0TgWCqRC8XjA==/109951171844247587.jpg", "320k"},

        {"66842", QStringLiteral("十年"), QStringLiteral("陈奕迅"), QStringLiteral("黑白灰"),
         "wy", 205, "https://p1.music.126.net/wECqsELfPFdVNLpW0QWhmA==/109951171836564936.jpg", "320k"},

        {"409931770", QStringLiteral("十面埋伏"), QStringLiteral("陈奕迅"), QStringLiteral("The Best Moment"),
         "wy", 232, "https://p1.music.126.net/kAKoBg1AOVrgvO4WEOywoA==/109951169594708968.jpg", "320k"},

        {"108485", QStringLiteral("Always Online"), QStringLiteral("林俊杰"), QStringLiteral("JJ陆"),
         "wy", 225, "https://p1.music.126.net/m1Jv3k4Yl4kpBv62NPjGHQ==/109951172814148796.jpg", "320k"},

        {"108914", QStringLiteral("江南"), QStringLiteral("林俊杰"), QStringLiteral("第二天堂"),
         "wy", 267, "https://p1.music.126.net/Gk4t93WwafRZtt9nTS77Iw==/109951171891430447.jpg", "320k"},

        {"108138", QStringLiteral("那些你很冒险的梦"), QStringLiteral("林俊杰"), QStringLiteral("学不会"),
         "wy", 244, "https://p1.music.126.net/Z5lg-iA7pGbquJvMoY8tbg==/109951168271379420.jpg", "320k"},

        {"26305547", QStringLiteral("背对背拥抱"), QStringLiteral("林俊杰"), QStringLiteral("他是…JJ林俊杰"),
         "wy", 236, "https://p1.music.126.net/Pz4sEpA7nsiyIdIrswTx9A==/109951167894836691.jpg", "320k"},

        {"2019573475", QStringLiteral("是你"), QStringLiteral("林俊杰"), QStringLiteral("JJ的咖啡调调,Vol.2"),
         "wy", 218, "https://p1.music.126.net/ssPAqFStzmN4KKsHQGrfbg==/109951169493493172.jpg", "320k"},
    };
}

#endif // SONGDATA_H
