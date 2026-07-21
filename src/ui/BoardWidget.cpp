#include "BoardWidget.h"

#include "Board.h"
#include "SpriteSheet.h"
#include "Sprites.h"

#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <climits>

namespace {

// Windows 7's default window is scale 1 (DEFAULT_WNDSCALE = 18 in the
// reference source), which is also the sprites' native size, so the
// default view is a pixel-perfect 1:1 blit.
constexpr int kDefaultCell = Sheet::kTile;
constexpr int kMinCell = 12;       // cells stay clickable when shrunk

constexpr int kTickMs = 30;        // shared animation clock, ~one frame per tick
constexpr int kRingStagger = 3;    // ticks between explosion distance rings
constexpr int kRippleTicks = 10;   // a flood uncovers over roughly this many
constexpr int kIntroMs = 550;      // deal-in length; Win7's 1100ms drags

// Widget size for a given cell size: the field plus the frame's gaps, which
// scale with the cells like the original's wndScale does. Rounded up so a
// window of exactly this size fits `cell`-sized tiles (flooring would make
// metrics() drop to cell - 1 and pad the matte instead).
QSize sizeFor(int cell, int rows, int cols)
{
    const qreal f = qreal(cell) / Sheet::kTile;
    return QSize(cell * cols
                     + qCeil((Sheet::kGapLeft + Sheet::kGapRight) * f),
                 cell * rows
                     + qCeil((Sheet::kGapTop + Sheet::kGapBottom) * f));
}

} // namespace

BoardWidget::BoardWidget(Board *board, QWidget *parent)
    : QWidget(parent), m_board(board)
{
    setMouseTracking(true);    // hover glow needs move events with no button
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_anim = new QTimer(this);
    m_anim->setInterval(kTickMs);
    connect(m_anim, &QTimer::timeout, this, &BoardWidget::animTick);

    connect(m_board, &Board::boardChanged, this, &BoardWidget::onBoardChanged);
    connect(m_board, &Board::gameEnded, this, [this](bool won) {
        if (!won)
            startExplosion();
    });
    connect(m_board, &Board::cellsOpened, this, &BoardWidget::startRipple);
}

void BoardWidget::onBoardChanged()
{
    // A reset (new game) arrives as a plain boardChanged; drop any running
    // or lingering animation with it.
    if (m_board->state() == Board::State::Ready)
        clearAnimations();
    update();
}

void BoardWidget::clearAnimations()
{
    m_boomActive = false;
    m_boomDone = false;
    m_introActive = false;
    m_ripple.clear();
    m_rippleHidden.clear();
    m_anim->stop();
}

void BoardWidget::playIntro()
{
    if (!m_animationsEnabled)
        return;
    const int rows = m_board->rows();
    const int cols = m_board->cols();
    if (rows <= 0 || cols <= 0)
        return;

    // Each cell pops with the diagonal wave of its nearest corner, the
    // waves running twice as fast along the board's longer side so all four
    // meet in the middle, the shape of the original's getFirstPaintNo().
    const int ratio = qMax(1, (qMax(rows, cols) + qMin(rows, cols) - 1)
                                  / qMin(rows, cols));
    m_introStart.fill(0, rows * cols);
    int maxWave = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int best = INT_MAX;
            for (int cr : {0, rows - 1}) {
                for (int cc : {0, cols - 1}) {
                    const int dMaj = cols >= rows ? qAbs(c - cc) : qAbs(r - cr);
                    const int dMin = cols >= rows ? qAbs(r - cr) : qAbs(c - cc);
                    best = qMin(best, (dMaj + ratio * dMin) / 2);
                }
            }
            m_introStart[r * cols + c] = best;
            maxWave = qMax(maxWave, best);
        }
    }

    // Spread the waves across kIntroMs, whatever the board size, scaled as
    // a float so a board with more waves than ticks packs several waves
    // into one tick instead of stretching the animation out.
    const qreal perWave = qreal(kIntroMs) / kTickMs / (maxWave + 1);
    for (int &start : m_introStart)
        start = int(start * perWave);
    m_introEnd = qMax(1, int((maxWave + 1) * perWave));

    m_introActive = true;
    m_tick = 0;
    m_anim->start();
}

void BoardWidget::startExplosion()
{
    const int rows = m_board->rows();
    const int cols = m_board->cols();
    m_boomActive = true;
    m_boomDone = false;
    m_tick = 0;
    m_ripple.clear();
    m_rippleHidden.clear();
    m_boomStart.fill(-1, rows * cols);

    // The mine that was hit detonates first...
    QPoint centre(-1, -1);
    for (int r = 0; r < rows && centre.x() < 0; ++r)
        for (int c = 0; c < cols && centre.x() < 0; ++c)
            if (m_board->at(r, c).exploded)
                centre = QPoint(c, r);

    // ...then the rest go off in rings radiating outward from it, every
    // mine in a ring together, like the original's breadth-first cascade.
    // Ring numbers are compressed so mine-free rings don't add dead time.
    QVector<int> rings;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const Cell &cell = m_board->at(r, c);
            if (!cell.mine || cell.mark == Mark::Flag)
                continue;
            if (cell.exploded) {
                m_boomStart[r * cols + c] = 0;
            } else {
                const int ring = qMax(qAbs(r - centre.y()),
                                      qAbs(c - centre.x()));
                m_boomStart[r * cols + c] = -2 - ring;   // patched below
                if (!rings.contains(ring))
                    rings.append(ring);
            }
        }
    }
    std::sort(rings.begin(), rings.end());
    m_boomEnd = Sheet::kBoomFrames;
    for (int i = 0; i < m_boomStart.size(); ++i) {
        if (m_boomStart[i] <= -2) {
            const int ring = -2 - m_boomStart[i];
            m_boomStart[i] = Sheet::kBoomFrames + 1
                             + int(rings.indexOf(ring)) * kRingStagger;
            m_boomEnd = qMax(m_boomEnd, m_boomStart[i] + Sheet::kBoomFrames);
        }
    }
    if (!m_animationsEnabled) {
        finishExplosionNow();   // straight to the smoking aftermath
        return;
    }
    m_anim->start();
}

void BoardWidget::finishExplosionNow()
{
    if (!m_boomActive || m_boomDone)
        return;
    m_tick = m_boomEnd;
    m_boomDone = true;
    if (m_ripple.isEmpty())
        m_anim->stop();
    update();
    emit explosionFinished();
}

void BoardWidget::startRipple(const QVector<QPoint> &order)
{
    if (!m_animationsEnabled)
        return;
    if (order.size() < 6)   // small opens just pop, like the original
        return;
    m_ripple = order;
    m_rippleShown = qMax(1, int(order.size()) / kRippleTicks);
    rebuildRippleHidden();
    m_anim->start();
}

void BoardWidget::rebuildRippleHidden()
{
    const int cols = m_board->cols();
    m_rippleHidden.clear();
    for (int i = m_rippleShown; i < m_ripple.size(); ++i)
        m_rippleHidden.insert(m_ripple[i].y() * cols + m_ripple[i].x());
}

void BoardWidget::animTick()
{
    ++m_tick;
    bool active = false;

    if (m_introActive) {
        if (m_tick >= m_introEnd)
            m_introActive = false;
        else
            active = true;
    }

    if (m_boomActive && !m_boomDone) {
        if (m_tick >= m_boomEnd) {
            m_boomDone = true;
            emit explosionFinished();
        } else {
            active = true;
        }
    }

    if (!m_ripple.isEmpty()) {
        m_rippleShown += qMax(1, int(m_ripple.size()) / kRippleTicks);
        if (m_rippleShown >= m_ripple.size()) {
            m_ripple.clear();
            m_rippleHidden.clear();
        } else {
            rebuildRippleHidden();
            active = true;
        }
    }

    if (!active)
        m_anim->stop();
    update();
}

void BoardWidget::setFlagOverlay(bool on)
{
    if (m_flagOverlay != on) {
        m_flagOverlay = on;
        update();
    }
}

void BoardWidget::setTimeSeconds(int seconds)
{
    if (m_seconds != seconds) {
        m_seconds = seconds;
        update();
    }
}

QSize BoardWidget::sizeHint() const
{
    return sizeFor(kDefaultCell, m_board->rows(), m_board->cols());
}

QSize BoardWidget::minimumSizeHint() const
{
    return sizeFor(kMinCell, m_board->rows(), m_board->cols());
}

QSize BoardWidget::snappedSize(const QSize &avail) const
{
    const int rows = m_board->rows();
    const int cols = m_board->cols();
    if (rows <= 0 || cols <= 0)
        return {};
    const qreal fitW = qreal(avail.width())
        / (Sheet::kTile * cols + Sheet::kGapLeft + Sheet::kGapRight);
    const qreal fitH = qreal(avail.height())
        / (Sheet::kTile * rows + Sheet::kGapTop + Sheet::kGapBottom);
    const int cell = qMax(kMinCell, int(Sheet::kTile * qMin(fitW, fitH)));
    return sizeFor(cell, rows, cols);
}

BoardWidget::Metrics BoardWidget::metrics() const
{
    Metrics m;
    const int rows = m_board->rows();
    const int cols = m_board->cols();
    if (rows <= 0 || cols <= 0)
        return m;

    // The zoom that fits field + gaps into the widget; the integer cell size
    // keeps sprite edges on pixel boundaries, and the gaps follow it.
    const qreal fitW = qreal(width())
        / (Sheet::kTile * cols + Sheet::kGapLeft + Sheet::kGapRight);
    const qreal fitH = qreal(height())
        / (Sheet::kTile * rows + Sheet::kGapTop + Sheet::kGapBottom);
    m.cell = qMax(8, int(Sheet::kTile * qMin(fitW, fitH)));
    m.f = qreal(m.cell) / Sheet::kTile;

    // Centre the field in the content area between the frame's edges.
    const qreal left = Sheet::kGapLeft * m.f;
    const qreal top = Sheet::kGapTop * m.f;
    const qreal contentW = width() - (Sheet::kGapLeft + Sheet::kGapRight) * m.f;
    const qreal contentH = height() - (Sheet::kGapTop + Sheet::kGapBottom) * m.f;
    m.grid = QRect(int(left + (contentW - m.cell * cols) / 2),
                   int(top + (contentH - m.cell * rows) / 2),
                   m.cell * cols, m.cell * rows);
    return m;
}

QPoint BoardWidget::cellAt(const QPoint &pos) const
{
    const Metrics m = metrics();
    if (m.grid.isEmpty() || !m.grid.contains(pos))
        return QPoint(-1, -1);
    return QPoint((pos.x() - m.grid.left()) / m.cell,
                  (pos.y() - m.grid.top()) / m.cell);
}

const QPixmap &BoardWidget::scaledSheet(int cell) const
{
    if (m_scaledCell != cell) {
        m_scaled = Sheet::scaledTiles(cell);
        m_scaledCell = cell;
    }
    return m_scaled;
}

void BoardWidget::paintBackdrop(QPainter &p, const Metrics &m) const
{
    const QPixmap &fr = Sheet::frame();
    const qreal f = m.f;
    const qreal w = width();
    const qreal h = height();
    const qreal L = Sheet::kGapLeft * f;
    const qreal T = Sheet::kGapTop * f;
    const qreal R = Sheet::kGapRight * f;
    const qreal B = Sheet::kGapBottom * f;
    const int sR = Sheet::kFrameW - Sheet::kGapRight;    // right strip source x
    const int sB = Sheet::kFrameH - Sheet::kGapBottom;   // bottom strip source y

    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Centre first, then the 9-patch edges. The centre is plain white in the
    // source, but black hides the sliver left uncovered when the window's
    // proportions drift a pixel from the field's. The edge strips are clean:
    // the sprites stashed in the source's centre sit outside every strip.
    p.fillRect(QRectF(L, T, w - L - R, h - T - B), Qt::black);
    const int eW = Sheet::kFrameW - Sheet::kGapLeft - Sheet::kGapRight;
    const int eH = Sheet::kFrameH - Sheet::kGapTop - Sheet::kGapBottom;
    p.drawPixmap(QRectF(0, 0, L, T), fr,
                 QRectF(0, 0, Sheet::kGapLeft, Sheet::kGapTop));
    p.drawPixmap(QRectF(w - R, 0, R, T), fr,
                 QRectF(sR, 0, Sheet::kGapRight, Sheet::kGapTop));
    p.drawPixmap(QRectF(0, h - B, L, B), fr,
                 QRectF(0, sB, Sheet::kGapLeft, Sheet::kGapBottom));
    p.drawPixmap(QRectF(w - R, h - B, R, B), fr,
                 QRectF(sR, sB, Sheet::kGapRight, Sheet::kGapBottom));
    p.drawPixmap(QRectF(L, 0, w - L - R, T), fr,
                 QRectF(Sheet::kGapLeft, 0, eW, Sheet::kGapTop));
    p.drawPixmap(QRectF(L, h - B, w - L - R, B), fr,
                 QRectF(Sheet::kGapLeft, sB, eW, Sheet::kGapBottom));
    p.drawPixmap(QRectF(0, T, L, h - T - B), fr,
                 QRectF(0, Sheet::kGapTop, Sheet::kGapLeft, eH));
    p.drawPixmap(QRectF(w - R, T, R, h - T - B), fr,
                 QRectF(sR, Sheet::kGapTop, Sheet::kGapRight, eH));

    // The status strip in the bottom border: clock button + time on the
    // left, mines-remaining + mine button on the right.
    const qreal button = 25 * f;
    const qreal boxW = 40 * f;
    const qreal boxH = 22 * f;
    const qreal inset = 20 * f;
    const qreal yButton = h - B + (B - button) / 2;
    const qreal yBox = h - B + (B - boxH) / 2;

    p.drawPixmap(QRectF(inset, yButton, button, button), Sheet::clockButton(),
                 Sheet::clockButton().rect());
    const QRectF timeBox(inset + button + 2 * f, yBox, boxW, boxH);
    p.drawPixmap(timeBox, Sheet::counterBox(), Sheet::counterBox().rect());
    p.drawPixmap(QRectF(w - inset - button, yButton, button, button),
                 Sheet::mineButton(), Sheet::mineButton().rect());
    const QRectF mineBox(w - inset - button - 2 * f - boxW, yBox, boxW, boxH);
    p.drawPixmap(mineBox, Sheet::counterBox(), Sheet::counterBox().rect());

    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(qMax(9, int(12 * f)));
    p.setFont(font);
    p.setPen(QColor(196, 218, 235));   // the original's counter text colour
    p.drawText(timeBox, Qt::AlignCenter, QString::number(m_seconds));
    p.drawText(mineBox, Qt::AlignCenter,
               QString::number(m_board->minesRemaining()));
}

void BoardWidget::paintEvent(QPaintEvent *)
{
    const Metrics m = metrics();
    QPainter p(this);
    paintBackdrop(p, m);
    if (m.grid.isEmpty())
        return;

    const QPixmap &sheet = scaledSheet(m.cell);
    const int cs = m.cell;
    const bool lost = m_board->state() == Board::State::Lost;
    const bool over = m_board->over();

    // A raised tile casts a shadow onto opened neighbours below/right of it;
    // the frame does the same along the field's top/left edge. Mines shown
    // on their tiles after a loss still count as raised; a misflag doesn't,
    // it opens into the crossed-out-mine floor sprite.
    const auto casts = [&](int r, int c) {
        const Cell &cell = m_board->at(r, c);
        return (!cell.revealed && !cell.misflagged)
               || (cell.revealed && cell.mine);
    };

    for (int r = 0; r < m_board->rows(); ++r) {
        for (int c = 0; c < m_board->cols(); ++c) {
            const QRect rect(m.grid.left() + c * cs, m.grid.top() + r * cs,
                             cs, cs);
            const Cell &cell = m_board->at(r, c);
            const int idx = r * m_board->cols() + c;

            // During the deal-in, tiles whose wave hasn't arrived still lie
            // pressed flat.
            if (m_introActive && !cell.revealed && m_introStart[idx] > m_tick) {
                p.drawPixmap(rect, sheet,
                             QRect(Sheet::BrickDown.col * cs,
                                   Sheet::BrickDown.row * cs, cs, cs));
                continue;
            }

            // A flood ripple keeps just-opened cells looking covered until
            // their breadth-first turn comes up.
            if (cell.revealed && !m_rippleHidden.isEmpty()
                && m_rippleHidden.contains(idx)) {
                p.drawPixmap(rect, sheet,
                             QRect(Sheet::Brick.col * cs,
                                   Sheet::Brick.row * cs, cs, cs));
                continue;
            }

            // A cascading mine draws its current explosion frame; frame 0
            // is the resting mine, so pre-detonation cells look normal.
            if (m_boomActive && cell.mine && cell.mark != Mark::Flag
                && m_boomStart[idx] >= 0) {
                const int f = qBound(0, m_tick - m_boomStart[idx],
                                     Sheet::kBoomFrames);
                const Sheet::Sprite s = cell.exploded ? Sheet::RedMineBoom(f)
                                                      : Sheet::MineBoom(f);
                p.drawPixmap(rect, sheet,
                             QRect(s.col * cs, s.row * cs, cs, cs));
                continue;
            }

            const bool pressTarget =
                m_mode != Press::None && m_press.x() >= 0
                && (m_mode == Press::Single
                        ? m_press == QPoint(c, r)
                        : qAbs(m_press.x() - c) <= 1 && qAbs(m_press.y() - r) <= 1);
            const bool hovered = !over && m_mode == Press::None
                                 && m_hover == QPoint(c, r);

            Sheet::Sprite s = Sheet::Brick;
            bool opened = false;   // drawn as floor -> receives shadows
            if (cell.misflagged) {
                s = Sheet::Misflag;
                opened = true;
            } else if (lost && cell.mine && cell.mark != Mark::Flag) {
                s = cell.exploded ? Sheet::RedMineBrick : Sheet::MineBrick;
            } else if (cell.revealed) {
                s = cell.adjacent > 0 ? Sheet::Number(cell.adjacent)
                                      : Sheet::Floor;
                opened = true;
            } else if (cell.mark == Mark::Flag) {
                s = pressTarget ? Sheet::FlagDown
                    : hovered   ? Sheet::FlagHover
                                : Sheet::Flag;
            } else if (cell.mark == Mark::Question) {
                s = pressTarget ? Sheet::QuestionDown
                    : hovered   ? Sheet::QuestionHover
                                : Sheet::Question;
                opened = pressTarget;
            } else {
                s = pressTarget ? Sheet::BrickDown
                    : hovered   ? Sheet::BrickHover
                                : Sheet::Brick;
                opened = pressTarget;
            }
            p.drawPixmap(rect, sheet,
                         QRect(s.col * cs, s.row * cs, cs, cs));

            if (!opened)
                continue;

            // The shadow strips live in sheet column 2, rows 1-3: top-only,
            // left-only, and the combined corner. 3px thick at native size.
            const bool top = (r == 0) || casts(r - 1, c);
            const bool left = (c == 0) || casts(r, c - 1);
            const qreal t = cs * 3.0 / Sheet::kTile;
            if (top && left) {
                p.drawPixmap(QRectF(rect.left(), rect.top(), cs, t), sheet,
                             QRectF(2 * cs, 3 * cs, cs, t));
                p.drawPixmap(QRectF(rect.left(), rect.top() + t, t, cs - t),
                             sheet, QRectF(2 * cs, 3 * cs + t, t, cs - t));
            } else if (top) {
                p.drawPixmap(QRectF(rect.left(), rect.top(), cs, t), sheet,
                             QRectF(2 * cs, 1 * cs, cs, t));
            } else if (left) {
                p.drawPixmap(QRectF(rect.left(), rect.top(), t, cs), sheet,
                             QRectF(2 * cs, 2 * cs, t, cs));
            }
        }
    }

    if (m_flagOverlay)
        Sprites::paintBosniaFlag(p, m.grid);
}

void BoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_introActive) {   // a click cuts the deal-in short and plays on
        m_introActive = false;
        update();
    }
    if (m_board->over()) {
        finishExplosionNow();   // a click skips the cascade, like Win7
        return;
    }
    const QPoint c = cellAt(event->pos());
    if (c.x() < 0)
        return;

    const Qt::MouseButtons held = event->buttons();
    if (event->button() == Qt::MiddleButton
        || (held.testFlag(Qt::LeftButton) && held.testFlag(Qt::RightButton))) {
        m_mode = Press::Chord;
        m_press = c;
    } else if (event->button() == Qt::LeftButton) {
        const Cell &cell = m_board->at(c.y(), c.x());
        if (!cell.revealed && cell.mark != Mark::Flag) {
            m_mode = Press::Single;
            m_press = c;
        }
    } else if (event->button() == Qt::RightButton) {
        m_board->toggleMark(c.y(), c.x());
    }
    update();
}

void BoardWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint c = cellAt(event->pos());
    bool dirty = false;
    if (c != m_hover) {
        m_hover = c;
        dirty = true;
    }
    // Sliding off the pressed tile cancels the pending reveal (Win7 lets you
    // bail out by dragging away before releasing).
    if (m_mode != Press::None && c != m_press) {
        m_press = c;
        dirty = true;
    }
    if (dirty)
        update();
}

void BoardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_mode == Press::Chord) {
        // Fire on the first button released; the second release finds
        // m_mode already back to None and does nothing.
        if (m_press.x() >= 0 && !m_board->over())
            m_board->chord(m_press.y(), m_press.x());
        m_mode = Press::None;
        m_press = QPoint(-1, -1);
    } else if (m_mode == Press::Single && event->button() == Qt::LeftButton) {
        const QPoint c = m_press;
        m_mode = Press::None;
        m_press = QPoint(-1, -1);
        if (c.x() >= 0 && !m_board->over())
            m_board->reveal(c.y(), c.x());
    }
    update();
}

void BoardWidget::leaveEvent(QEvent *)
{
    if (m_hover.x() >= 0) {
        m_hover = QPoint(-1, -1);
        update();
    }
}
