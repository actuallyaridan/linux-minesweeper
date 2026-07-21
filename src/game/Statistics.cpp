#include "Statistics.h"

#include <QSettings>

namespace Stats {

static QString key(const QString &id, const char *field)
{
    return QStringLiteral("stats/%1/%2").arg(id, QLatin1String(field));
}

Data load(const QString &difficultyId)
{
    QSettings s;
    Data d;
    d.played = s.value(key(difficultyId, "played")).toInt();
    d.won = s.value(key(difficultyId, "won")).toInt();
    d.bestTime = s.value(key(difficultyId, "bestTime")).toInt();
    d.curStreak = s.value(key(difficultyId, "curStreak")).toInt();
    d.bestWinStreak = s.value(key(difficultyId, "bestWinStreak")).toInt();
    d.worstLossStreak = s.value(key(difficultyId, "worstLossStreak")).toInt();
    return d;
}

bool recordGame(const QString &difficultyId, bool won, int seconds)
{
    Data d = load(difficultyId);
    ++d.played;
    bool record = false;
    if (won) {
        ++d.won;
        d.curStreak = d.curStreak > 0 ? d.curStreak + 1 : 1;
        d.bestWinStreak = qMax(d.bestWinStreak, d.curStreak);
        if (d.bestTime == 0 || seconds < d.bestTime) {
            d.bestTime = seconds;
            record = true;
        }
    } else {
        d.curStreak = d.curStreak < 0 ? d.curStreak - 1 : -1;
        d.worstLossStreak = qMax(d.worstLossStreak, -d.curStreak);
    }

    QSettings s;
    s.setValue(key(difficultyId, "played"), d.played);
    s.setValue(key(difficultyId, "won"), d.won);
    s.setValue(key(difficultyId, "bestTime"), d.bestTime);
    s.setValue(key(difficultyId, "curStreak"), d.curStreak);
    s.setValue(key(difficultyId, "bestWinStreak"), d.bestWinStreak);
    s.setValue(key(difficultyId, "worstLossStreak"), d.worstLossStreak);
    return record;
}

void reset()
{
    QSettings().remove(QStringLiteral("stats"));
}

} // namespace Stats
