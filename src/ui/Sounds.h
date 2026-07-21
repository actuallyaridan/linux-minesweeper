#pragma once

// The original Windows 7 Minesweeper sounds (from the same Lmy0217 assets as
// the sprites), mapped the way that game plays them:
//
//  firstPaint.wav          a new board being dealt
//  zeroDevelop.wav         a flood-open rippling across empty squares
//  dbButtonClickFlag.wav   planting or clearing a flag
//  mineBombStart/Run/End   the loss explosion, three samples chained
//
// Header-only and moc-free: QSoundEffect is already a QObject, so the chain
// connections just borrow its signals.

#include <QObject>
#include <QSoundEffect>
#include <QUrl>

class Sounds {
public:
    Sounds()
    {
        setup(m_newGame, "firstPaint.wav");
        setup(m_flood, "zeroDevelop.wav");
        setup(m_flag, "dbButtonClickFlag.wav");
        setup(m_boomStart, "mineBombStart.wav");
        setup(m_boomRun, "mineBombRun.wav");
        setup(m_boomEnd, "mineBombEnd.wav");

        // The staged explosion: each sample starts when the previous one
        // finishes, unless stopAll() (a new game) broke the chain.
        QObject::connect(&m_boomStart, &QSoundEffect::playingChanged,
                         &m_boomStart, [this] {
            if (!m_boomStart.isPlaying() && m_chainStage == 1) {
                m_chainStage = 2;
                m_boomRun.play();
            }
        });
        QObject::connect(&m_boomRun, &QSoundEffect::playingChanged,
                         &m_boomRun, [this] {
            if (!m_boomRun.isPlaying() && m_chainStage == 2) {
                m_chainStage = 0;
                m_boomEnd.play();
            }
        });
    }

    void setEnabled(bool on) { m_enabled = on; }
    bool enabled() const { return m_enabled; }

    void newGame() { play(m_newGame); }
    void flood() { play(m_flood); }
    void flag() { play(m_flag); }

    // `followUpMines` is how many mines detonate after the first one: the
    // run sample loops once per mine while the cascade plays them out.
    void explosion(int followUpMines)
    {
        if (!m_enabled)
            return;
        m_boomRun.setLoopCount(qMax(1, followUpMines));
        m_chainStage = 1;
        m_boomStart.play();
    }

    // The cascade animation is over (or was skipped): cut the remaining run
    // loops short and let the chain drop straight to the closing boom.
    void finishExplosion()
    {
        if (m_chainStage == 2 && m_boomRun.isPlaying())
            m_boomRun.stop();
        else if (m_chainStage == 1)
            m_boomRun.setLoopCount(1);
    }

    void stopAll()
    {
        m_chainStage = 0;
        for (QSoundEffect *e : {&m_newGame, &m_flood, &m_flag, &m_boomStart,
                                &m_boomRun, &m_boomEnd})
            e->stop();
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

    QSoundEffect m_newGame, m_flood, m_flag;
    QSoundEffect m_boomStart, m_boomRun, m_boomEnd;
    int m_chainStage = 0;
    bool m_enabled = true;
};
