#pragma once

// The original Windows 7 Minesweeper sounds, mapped to the roles the
// game's own MINESWEEPER.xml gives them:
//
//  gameStart.wav        NewBoard      a fresh board being dealt
//  tileSingle.wav       SingleReveal  one square opening
//  tileMultiple.wav     MultiReveal   a flood opening several squares
//  gameWin.wav          BoardCleared  the board is cleared
//  gameLose.wav         WrongGuess    a mine detonating; the original keeps
//                                     30 buffers so cascading mines overlap,
//                                     mirrored here with a small pool
//  invalidMove.wav      InvalidMove   a click that can't do anything
//  flowerLose*.wav      WrongGuess    the Flower Garden loss, one sample
//                                     only (PlayOnlyFirstLoseSound), sized
//                                     to the cascade
//  dbButtonClickFlag.wav              planting or clearing a flag
//
// Header-only and moc-free; QSoundEffect is already a QObject.

#include <QSoundEffect>
#include <QUrl>

class Sounds {
public:
    enum class LoseStyle { Mines, Flowers };

    Sounds()
    {
        setup(m_newGame, "gameStart.wav");
        setup(m_single, "tileSingle.wav");
        setup(m_multi, "tileMultiple.wav");
        setup(m_win, "gameWin.wav");
        setup(m_invalid, "invalidMove.wav");
        setup(m_flag, "dbButtonClickFlag.wav");
        setup(m_flowerShort, "flowerLoseShort.wav");
        setup(m_flowerMedium, "flowerLoseMedium.wav");
        setup(m_flowerLong, "flowerLoseLong.wav");
        for (QSoundEffect &e : m_losePool)
            setup(e, "gameLose.wav");
    }

    void setEnabled(bool on) { m_enabled = on; }
    bool enabled() const { return m_enabled; }
    void setLoseStyle(LoseStyle s) { m_loseStyle = s; }

    void newGame() { play(m_newGame); }
    void singleReveal() { play(m_single); }
    void multiReveal() { play(m_multi); }
    void flag() { play(m_flag); }
    void win() { play(m_win); }
    void invalidMove() { play(m_invalid); }

    // The loss is starting; `doomedMines` is how many will detonate after
    // the first. Flower Garden plays one wilting sample sized to the
    // cascade and nothing per-mine; Minesweeper booms on every trip.
    void beginLoss(int doomedMines)
    {
        if (m_loseStyle == LoseStyle::Flowers) {
            play(doomedMines <= 10   ? m_flowerShort
                 : doomedMines <= 40 ? m_flowerMedium
                                     : m_flowerLong);
        } else {
            mineTripped();
        }
    }

    // Another mine (or ring of mines) went off mid-cascade.
    void mineTripped()
    {
        if (!m_enabled || m_loseStyle == LoseStyle::Flowers)
            return;
        m_losePool[m_nextLose].play();   // overlaps earlier booms
        m_nextLose = (m_nextLose + 1) % kLosePool;
    }

    void stopAll()
    {
        for (QSoundEffect *e : {&m_newGame, &m_single, &m_multi, &m_win,
                                &m_invalid, &m_flag, &m_flowerShort,
                                &m_flowerMedium, &m_flowerLong})
            e->stop();
        for (QSoundEffect &e : m_losePool)
            e.stop();
    }

private:
    void setup(QSoundEffect &e, const char *name)
    {
        e.setSource(QUrl(QStringLiteral("qrc:/assets/") + QLatin1String(name)));
        e.setVolume(0.9);
    }

    void play(QSoundEffect &e)
    {
        if (m_enabled) {
            e.stop();   // retrigger from the top on rapid repeats
            e.play();
        }
    }

    static constexpr int kLosePool = 4;

    QSoundEffect m_newGame, m_single, m_multi, m_win, m_invalid, m_flag;
    QSoundEffect m_flowerShort, m_flowerMedium, m_flowerLong;
    QSoundEffect m_losePool[kLosePool];
    int m_nextLose = 0;
    LoseStyle m_loseStyle = LoseStyle::Mines;
    bool m_enabled = true;
};
