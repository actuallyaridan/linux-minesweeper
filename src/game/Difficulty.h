#pragma once

#include <QString>

// A board configuration. The three presets match Windows 7 exactly; "custom"
// carries whatever the user typed into the Options dialog. The id doubles as
// the QSettings key prefix for the per-difficulty statistics, which is why
// custom games are never recorded (same as Windows 7).
struct Difficulty {
    QString id;
    int rows = 9;
    int cols = 9;
    int mines = 10;
};

namespace Difficulties {

inline const Difficulty Beginner{QStringLiteral("beginner"), 9, 9, 10};
inline const Difficulty Intermediate{QStringLiteral("intermediate"), 16, 16, 40};
inline const Difficulty Advanced{QStringLiteral("advanced"), 16, 30, 99};

// Not a Windows 7 preset. 26.7% mine density against Advanced's 20.6%:
// winnable with disciplined chording and endgame mine-counting, but only
// just (community "evil" boards top out around 25%). The 192 is for 1992.
inline const Difficulty Bosnia{QStringLiteral("bosnia"), 24, 30, 192};

} // namespace Difficulties
