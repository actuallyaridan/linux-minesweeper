#pragma once

#include <QString>

// Per-difficulty lifetime statistics, persisted through QSettings under
// stats/<difficulty id>/. Stateless free functions (Win7Ui.h style): every
// call reads or writes the settings store directly, so there is no cache to
// fall out of sync when the Statistics dialog and the game both touch it.
namespace Stats {

struct Data {
    int played = 0;
    int won = 0;
    int bestTime = 0;         // seconds; 0 = no win recorded yet
    int curStreak = 0;        // positive = wins in a row, negative = losses
    int bestWinStreak = 0;
    int worstLossStreak = 0;
};

Data load(const QString &difficultyId);

// Returns true when a won game set a new best time (drives the "new record"
// line in the game-won dialog).
bool recordGame(const QString &difficultyId, bool won, int seconds);

void reset();

} // namespace Stats
