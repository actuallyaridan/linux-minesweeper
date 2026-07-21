#pragma once

#include <QPixmap>
#include <QPoint>
#include <QSet>
#include <QVector>
#include <QWidget>

class Board;
class QTimer;

// The whole play area, drawn the way the original does it: the Frame.bmp
// backdrop stretched as a 9-patch over the widget, the minefield blitted
// from the Windows 7 sprite sheet in the middle, and the clock/mine buttons
// with their navy counters in the bottom border strip. Everything scales
// together with the window, tracking the original's wndScale behaviour
// (18px sprites at scale 1).
class BoardWidget : public QWidget {
    Q_OBJECT
public:
    explicit BoardWidget(Board *board, QWidget *parent = nullptr);

    // The faint Bosnian flag painted over the field on the Bosnia difficulty.
    void setFlagOverlay(bool on);
    // Shown in the left counter; the game clock lives in MainWindow.
    void setTimeSeconds(int seconds);
    // Options > Display animations: off skips the flood ripple and jumps
    // the loss cascade straight to its aftermath.
    void setAnimationsEnabled(bool on) { m_animationsEnabled = on; }

    // The Win7 deal-in: the fresh board starts pressed flat and the tiles
    // pop up in diagonal waves from all four corners (~1.1s, in step with
    // firstPaint.wav). MainWindow calls this after each reset/restart.
    void playIntro();

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
    const QPixmap &scaledSheet(int cell) const;
    void paintBackdrop(QPainter &p, const Metrics &m) const;

    void onBoardChanged();
    void startExplosion();
    void finishExplosionNow();
    void startRipple(const QVector<QPoint> &order);
    void rebuildRippleHidden();   // cells from m_rippleShown on, still covered
    void animTick();
    void clearAnimations();

    Board *m_board;
    QPoint m_hover{-1, -1};
    QPoint m_press{-1, -1};
    Press m_mode = Press::None;
    bool m_flagOverlay = false;
    bool m_animationsEnabled = true;
    int m_seconds = 0;

    // The Win7 animations, both driven by one ~30ms timer. The explosion
    // cascade gives every doomed mine a start tick (rings radiating out from
    // the one that was hit); the flood ripple hides just-opened cells and
    // uncovers them a few per tick in breadth-first order.
    QTimer *m_anim;
    int m_tick = 0;
    bool m_boomActive = false;
    bool m_boomDone = false;
    int m_boomEnd = 0;
    QVector<int> m_boomStart;          // per cell index; -1 = not animating
    QVector<QPoint> m_ripple;
    int m_rippleShown = 0;
    QSet<int> m_rippleHidden;
    bool m_introActive = false;
    int m_introEnd = 0;
    QVector<int> m_introStart;         // per cell: tick its tile pops up

    // One scaled copy of the sprite sheet, rebuilt only when the cell size
    // changes (i.e. on resize), not on every repaint.
    mutable QPixmap m_scaled;
    mutable int m_scaledCell = 0;
};
