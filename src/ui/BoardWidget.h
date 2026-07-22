#pragma once

#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QVector>
#include <QWidget>

#include "Theme.h"

class Board;
class QTimer;

// The whole play area, drawn the way the original does it: the Frame.bmp
// backdrop stretched as a 9-patch over the widget, the minefield blitted
// from the Windows 7 sheets (gradient tiles per board style, mines or
// flowers per game style), and the clock/mine counters in the bottom
// border strip. Everything scales together with the window, tracking the
// original's wndScale behaviour (18px sprites at scale 1).
class BoardWidget : public QWidget {
    Q_OBJECT
public:
    explicit BoardWidget(Board *board, QWidget *parent = nullptr);

    // Windows 7's Change Appearance: which tile sheet and which thing
    // hides under the tiles.
    void setStyles(Theme::BoardStyle board, Theme::GameStyle game);
    Theme::BoardStyle boardStyle() const { return m_boardStyle; }
    Theme::GameStyle gameStyle() const { return m_gameStyle; }

    // The faint Bosnian flag painted over the field on the Bosnia difficulty.
    void setFlagOverlay(bool on);
    // Shown in the left counter; the game clock lives in MainWindow.
    void setTimeSeconds(int seconds);
    // Options > Display animations: off skips the flood ripple and jumps
    // the loss cascade / win sweep straight to their aftermath.
    void setAnimationsEnabled(bool on) { m_animationsEnabled = on; }

    // The Win7 deal-in: the fresh board starts pressed flat and the tiles
    // pop up in diagonal waves from all four corners. MainWindow calls this
    // after each reset/restart.
    void playIntro();
    // The win sweep: the scan bar rises over the field and every mine
    // plays its disarm beam as the bar passes. Ends with winFinished().
    void playWin();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // The nearest size at or below `avail` whose proportions match the play
    // area exactly (whole-pixel cells, gaps scaled to them). MainWindow snaps
    // window resizes through this so the frame always hugs the field.
    QSize snappedSize(const QSize &avail) const;

signals:
    // The loss cascade has finished (or was skipped with a click); the
    // game-lost dialog waits for this.
    void explosionFinished();
    // The win sweep has finished (or was skipped); ditto the won dialog.
    void winFinished();
    // A ring of mines detonated mid-cascade; MainWindow answers with an
    // overlapping boom, the way the original keeps 30 sound buffers.
    void mineTripped();
    // A click that couldn't do anything (a chord on the wrong flag count).
    void invalidMove();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // Held-button state: Single = left button pressing one covered tile,
    // Chord = middle button (or left+right) pressing a number and its ring.
    enum class Press { None, Single, Chord };

    // The current scale: f is the zoom relative to the 18px sprites
    // (the original's wndScale / 18), grid the minefield's pixel rect.
    struct Metrics {
        qreal f = 1.0;
        int cell = 18;
        QRect grid;
    };
    Metrics metrics() const;
    QPoint cellAt(const QPoint &pos) const;   // x = column, y = row; (-1,-1) outside

    // Every sprite pre-scaled for one cell size; rebuilt on resize or when
    // the styles change, not on every repaint. The tile blocks hold all 720
    // gradient frames, addressed frame -> (frame%25, frame/25) like the
    // sheet itself.
    struct SpriteSet {
        int cell = 0;
        QPixmap normal, pressed;              // 25 x 29-frame blocks
        QPixmap flag, question, hilite, misflagX;
        QPixmap shadowU, shadowL;
        QPixmap digits[8];
        QPixmap mine;
        QPixmap crater, craterRed;   // craterRed: the one that was clicked
    };
    const SpriteSet &sprites() const;
    QRect tileSrc(const QPixmap &block, int frame) const;

    void paintBackdrop(QPainter &p, const Metrics &m) const;
    void paintCell(QPainter &p, const Metrics &m, int r, int c) const;
    void paintWinSweep(QPainter &p, const Metrics &m) const;

    void onBoardChanged();
    void startExplosion();
    void finishExplosionNow();
    void finishWinNow();
    void startRipple(const QVector<QPoint> &order);
    void animTick();
    void clearAnimations();

    Board *m_board;
    Theme::BoardStyle m_boardStyle = Theme::BoardStyle::Blue;
    Theme::GameStyle m_gameStyle = Theme::GameStyle::Mines;
    QPoint m_hover{-1, -1};
    QPoint m_press{-1, -1};
    Press m_mode = Press::None;
    bool m_flagOverlay = false;
    bool m_animationsEnabled = true;
    int m_seconds = 0;

    // The Win7 animations, all driven by one ~30ms timer. The explosion
    // cascade gives every doomed mine a start tick (rings radiating out
    // from the one that was hit); the flood ripple hides just-opened cells
    // and uncovers them a few per tick in breadth-first order; the win
    // sweep starts each mine's disarm beam as the scan bar passes its row.
    QTimer *m_anim;
    int m_tick = 0;
    bool m_boomActive = false;
    bool m_boomDone = false;
    int m_boomEnd = 0;
    QVector<int> m_boomStart;          // per cell index; -1 = not animating
    QVector<int> m_ringStarts;         // ticks that detonate another ring
    bool m_winActive = false;
    bool m_winDone = false;
    int m_winEnd = 0;
    QVector<int> m_disarmStart;        // per cell index; -1 = no mine
    QHash<int, int> m_rippleStart;     // cell index -> tick its tile fades
    int m_rippleEnd = 0;
    bool m_introActive = false;
    int m_introEnd = 0;
    QVector<int> m_introStart;         // per cell: tick its tile pops up

    mutable SpriteSet m_sprites;
};
