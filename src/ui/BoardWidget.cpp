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
constexpr int kDefaultCell = Theme::kTile;
constexpr int kMinCell = 12;       // cells stay clickable when shrunk

constexpr int kTickMs = 30;        // shared animation clock, ~one frame per tick
constexpr int kRingStagger = 3;    // ticks between explosion distance rings
constexpr int kIntroMs = 700;      // deal-in wave spread; Win7's 1100ms drags

// A flood-open melts outward from the click: the ripple ring advances a
// tick per cell of distance, and each tile it reaches dissolves with a
// quick linear alpha ramp (the shape of animationAlphaQuickFadeOut, at a
// brisker clip than its half second).
constexpr int kRippleStagger = 1;
constexpr int kRippleFadeTicks = 150 / kTickMs;

// Each tile fades in once its wave arrives, the shape of the original's
// animationAlphaQuickFadeIn, quickened to match the tighter wave spread.
// The waves overlap, so the front reads as a soft shimmer instead of a
// hard edge.
constexpr int kIntroFadeTicks = 300 / kTickMs;

// The animation XMLs' timings, in ticks: the mine trip runs over one
// second, the disarm beam its 38 frames over two, and the scan bar
// sweeps the field in three.
constexpr int kTripTicks = 1000 / kTickMs;
constexpr int kDisarmTicks = 2000 / kTickMs;
constexpr int kScanTicks = 3000 / kTickMs;

// Widget size for a given cell size: the field plus the frame's gaps, which
// scale with the cells like the original's wndScale does. Rounded up so a
// window of exactly this size fits `cell`-sized tiles (flooring would make
// metrics() drop to cell - 1 and pad the matte instead).
QSize sizeFor(int cell, int rows, int cols)
{
    const qreal f = qreal(cell) / Theme::kTile;
    return QSize(cell * cols
                     + qCeil((Sheet::kGapLeft + Sheet::kGapRight) * f),
                 cell * rows
                     + qCeil((Sheet::kGapTop + Sheet::kGapBottom) * f));
}

// One sprite cut from a sheet and smooth-scaled on its own, so the filter
// can't bleed a neighbouring sprite into its edge pixels.
QPixmap cutSprite(const QPixmap &sheet, const QRect &logical,
                  const QSize &target)
{
    const int k = Theme::kScale;
    return sheet
        .copy(logical.x() * k, logical.y() * k, logical.width() * k,
              logical.height() * k)
        .scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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

void BoardWidget::setStyles(Theme::BoardStyle board, Theme::GameStyle game)
{
    if (m_boardStyle == board && m_gameStyle == game)
        return;
    m_boardStyle = board;
    m_gameStyle = game;
    m_sprites.cell = 0;   // sprites belong to the old sheets; rebuild
    update();
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
    m_winActive = false;
    m_winDone = false;
    m_introActive = false;
    m_rippleStart.clear();
    m_ringStarts.clear();
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
    // The board is dealt once the last wave has arrived and faded in.
    m_introEnd = qMax(1, int((maxWave + 1) * perWave)) + kIntroFadeTicks;

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
    m_rippleStart.clear();
    m_ringStarts.clear();
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
    // Each ring's mines explode together, and the cascade is over when
    // the last explosion has burnt through its second.
    m_boomEnd = kTripTicks;
    for (int i = 0; i < m_boomStart.size(); ++i) {
        if (m_boomStart[i] <= -2) {
            const int ring = -2 - m_boomStart[i];
            const int start = kRingStagger * (1 + int(rings.indexOf(ring)));
            m_boomStart[i] = start;
            if (!m_ringStarts.contains(start))
                m_ringStarts.append(start);
            m_boomEnd = qMax(m_boomEnd, start + kTripTicks);
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
    m_ringStarts.clear();
    m_anim->stop();
    update();
    emit explosionFinished();
}

void BoardWidget::playWin()
{
    const int rows = m_board->rows();
    const int cols = m_board->cols();
    m_winActive = true;
    m_winDone = false;
    m_tick = 0;
    m_rippleStart.clear();

    // Each mine's disarm beam fires as the scan bar passes its row on the
    // way up; the sweep is over when the bar tops out and the last beam
    // has burnt through its two seconds.
    m_disarmStart.fill(-1, rows * cols);
    m_winEnd = kScanTicks;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!m_board->at(r, c).mine)
                continue;
            const int start = kScanTicks * (rows - r) / (rows + 1);
            m_disarmStart[r * cols + c] = start;
            m_winEnd = qMax(m_winEnd, start + kDisarmTicks);
        }
    }
    if (!m_animationsEnabled) {
        finishWinNow();
        return;
    }
    m_anim->start();
}

void BoardWidget::finishWinNow()
{
    if (!m_winActive || m_winDone)
        return;
    m_tick = m_winEnd;
    m_winDone = true;
    m_anim->stop();
    update();
    emit winFinished();
}

void BoardWidget::startRipple(const QVector<QPoint> &order)
{
    if (!m_animationsEnabled)
        return;
    if (order.size() < 6)   // small opens just pop, like the original
        return;

    // Each opened tile dissolves with the original's quick alpha fade,
    // starting when the ripple ring radiating out from the click reaches
    // it, so the flood melts outward instead of popping in chunks.
    if (!m_anim->isActive())
        m_tick = 0;
    const int cols = m_board->cols();
    const QPoint origin = order.first();
    m_rippleEnd = m_tick;
    for (const QPoint &pt : order) {
        const int ring = qMax(qAbs(pt.x() - origin.x()),
                              qAbs(pt.y() - origin.y()));
        const int start = m_tick + ring * kRippleStagger;
        m_rippleStart.insert(pt.y() * cols + pt.x(), start);
        m_rippleEnd = qMax(m_rippleEnd, start + kRippleFadeTicks);
    }
    m_anim->start();
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
        if (m_ringStarts.contains(m_tick))
            emit mineTripped();
        if (m_tick >= m_boomEnd) {
            m_boomDone = true;
            emit explosionFinished();
        } else {
            active = true;
        }
    }

    if (m_winActive && !m_winDone) {
        if (m_tick >= m_winEnd) {
            m_winDone = true;
            emit winFinished();
        } else {
            active = true;
        }
    }

    if (!m_rippleStart.isEmpty()) {
        if (m_tick >= m_rippleEnd)
            m_rippleStart.clear();
        else
            active = true;
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
        / (Theme::kTile * cols + Sheet::kGapLeft + Sheet::kGapRight);
    const qreal fitH = qreal(avail.height())
        / (Theme::kTile * rows + Sheet::kGapTop + Sheet::kGapBottom);
    const int cell = qMax(kMinCell, int(Theme::kTile * qMin(fitW, fitH)));
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
        / (Theme::kTile * cols + Sheet::kGapLeft + Sheet::kGapRight);
    const qreal fitH = qreal(height())
        / (Theme::kTile * rows + Sheet::kGapTop + Sheet::kGapBottom);
    m.cell = qMax(8, int(Theme::kTile * qMin(fitW, fitH)));
    m.f = qreal(m.cell) / Theme::kTile;

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

const BoardWidget::SpriteSet &BoardWidget::sprites() const
{
    const int cell = metrics().cell;
    if (m_sprites.cell == cell)
        return m_sprites;

    const QPixmap &board = Theme::boardSheet(m_boardStyle);
    const QPixmap &game = Theme::gameSheet(m_gameStyle);
    SpriteSet &S = m_sprites;
    S.cell = cell;

    // The two 720-frame gradient blocks are scaled in one piece: adjacent
    // frames are near-identical colours, so the filter's cross-frame
    // sampling is invisible, and one big scale beats 1440 little ones.
    S.normal = cutSprite(board, QRect(0, 0, 25 * 18, 29 * 18),
                         QSize(25 * cell, 29 * cell));
    S.pressed = cutSprite(board, QRect(500, 0, 25 * 18, 29 * 18),
                          QSize(25 * cell, 29 * cell));

    const QSize tile(cell, cell);
    S.flag = cutSprite(board, Theme::kFlag, tile);
    S.question = cutSprite(board, Theme::kQuestion, tile);
    S.hilite = cutSprite(board, Theme::kHilite, tile);
    S.misflagX = cutSprite(board, Theme::kMisflagX, tile);
    S.mine = cutSprite(game, Theme::kMine, tile);

    // What a detonated mine (or tripped flower) leaves behind: a faint,
    // washed-out ghost of the sprite, grey for most and a soft red wash
    // for the one that was clicked. The alpha is reapplied after the
    // multiply (which would otherwise colour the clear parts), then thinned.
    const auto remnant = [&](const QColor &wash, bool drain, int alpha) {
        QImage img = S.mine.toImage().convertToFormat(QImage::Format_ARGB32);
        if (drain) {   // drain the colour to grey, pixel by pixel
            for (int y = 0; y < img.height(); ++y) {
                QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
                for (int x = 0; x < img.width(); ++x) {
                    const int g = qGray(line[x]);
                    line[x] = qRgba(g, g, g, qAlpha(line[x]));
                }
            }
        }
        QPixmap out = QPixmap::fromImage(img);
        QPainter tint(&out);
        tint.setCompositionMode(QPainter::CompositionMode_Multiply);
        tint.fillRect(out.rect(), wash);
        tint.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        tint.drawPixmap(0, 0, S.mine);
        tint.fillRect(out.rect(), QColor(0, 0, 0, alpha));
        return out;
    };
    S.crater = remnant(QColor(175, 175, 175), true, 140);
    S.craterRed = remnant(QColor(255, 96, 96), false, 160);

    const int t = qMax(1, qRound(cell * 3.0 / Theme::kTile));
    S.shadowU = cutSprite(board, Theme::kShadowU, QSize(cell, t));
    S.shadowL = cutSprite(board, Theme::kShadowL, QSize(t, cell));

    const qreal f = qreal(cell) / Theme::kTile;
    for (int n = 1; n <= 8; ++n)
        S.digits[n - 1] = cutSprite(board, Theme::digit(n),
                                    QSize(qMax(2, qRound(10 * f)),
                                          qMax(2, qRound(13 * f))));
    return S;
}

QRect BoardWidget::tileSrc(const QPixmap &, int frame) const
{
    const int cell = m_sprites.cell;
    return QRect(cell * (frame % 25), cell * (frame / 25), cell, cell);
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
    // proportions drift a pixel from the field's.
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

    // The status strip in the bottom border, laid out like the original's
    // MINESWEEPER.xml CustomLayout: X offsets from the window's centre,
    // Y offsets from its bottom. Clock icon and time on the left, mine
    // count and the game style's mine (or flower) icon on the right.
    const QPixmap &other = Theme::otherSheet();
    const QPixmap &game = Theme::gameSheet(m_gameStyle);
    const QPixmap &board = Theme::boardSheet(m_boardStyle);
    const int k = Theme::kScale;
    const qreal cx = w / 2;

    const auto place = [&](qreal x, qreal y, const QRect &logical) {
        return QRectF(cx + x * f, h + y * f, logical.width() * f,
                      logical.height() * f);
    };
    p.drawPixmap(place(-90, -30, Theme::kClock), other,
                 QRectF(Theme::kClock.x() * k, Theme::kClock.y() * k,
                        Theme::kClock.width() * k, Theme::kClock.height() * k));
    p.drawPixmap(place(64, -30, Theme::kPanelMine), game,
                 QRectF(Theme::kPanelMine.x() * k, Theme::kPanelMine.y() * k,
                        Theme::kPanelMine.width() * k,
                        Theme::kPanelMine.height() * k));
    const QRectF panelSrc(Theme::kCounterPanel.x() * k,
                          Theme::kCounterPanel.y() * k,
                          Theme::kCounterPanel.width() * k,
                          Theme::kCounterPanel.height() * k);
    const QRectF timeBox = place(-59, -26, Theme::kCounterPanel);
    const QRectF mineBox = place(18, -26, Theme::kCounterPanel);
    p.drawPixmap(timeBox, board, panelSrc);
    p.drawPixmap(mineBox, board, panelSrc);

    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(qMax(9, int(12 * f)));
    p.setFont(font);
    p.setPen(Qt::white);   // PanelLabelTextColor in the original's config
    p.drawText(timeBox, Qt::AlignCenter, QString::number(m_seconds));
    p.drawText(mineBox, Qt::AlignCenter,
               QString::number(m_board->minesRemaining()));
}

void BoardWidget::paintCell(QPainter &p, const Metrics &m, int r, int c) const
{
    const SpriteSet &S = sprites();
    const int cs = m.cell;
    const QRect rect(m.grid.left() + c * cs, m.grid.top() + r * cs, cs, cs);
    const Cell &cell = m_board->at(r, c);
    const int idx = r * m_board->cols() + c;
    const int frame = Theme::gradientFrame(r, c, m_board->rows(),
                                           m_board->cols());
    const bool over = m_board->over();

    // During the deal-in, tiles whose wave hasn't arrived still lie
    // pressed flat; when it does, each tile fades in over the flat floor,
    // like the original's quick alpha fade.
    if (m_introActive && !cell.revealed) {
        const int dt = m_tick - m_introStart[idx];
        if (dt < kIntroFadeTicks) {
            p.drawPixmap(rect, S.pressed, tileSrc(S.pressed, frame));
            if (dt > 0) {
                p.setOpacity(qreal(dt) / kIntroFadeTicks);
                p.drawPixmap(rect, S.normal, tileSrc(S.normal, frame));
                p.setOpacity(1.0);
            }
            return;
        }
    }

    // A flood ripple: just-opened cells stay covered until the ring
    // radiating from the click reaches them, then their tile dissolves
    // over the floor (`cover` is the fading tile's remaining opacity,
    // painted over the opened cell at the end).
    qreal cover = 0.0;
    if (cell.revealed && !m_rippleStart.isEmpty()) {
        const auto it = m_rippleStart.constFind(idx);
        if (it != m_rippleStart.constEnd()) {
            const int dt = m_tick - it.value();
            if (dt <= 0) {
                p.drawPixmap(rect, S.normal, tileSrc(S.normal, frame));
                return;
            }
            if (dt < kRippleFadeTicks)
                cover = 1.0 - qreal(dt) / kRippleFadeTicks;
        }
    }

    if (!cell.revealed) {
        const bool pressTarget =
            !over && m_mode != Press::None && m_press.x() >= 0
            && cell.mark != Mark::Flag
            && (m_mode == Press::Single
                    ? m_press == QPoint(c, r)
                    : qAbs(m_press.x() - c) <= 1 && qAbs(m_press.y() - r) <= 1);
        const bool hovered = !over && m_mode == Press::None
                             && m_hover == QPoint(c, r);

        if (pressTarget) {
            p.drawPixmap(rect, S.pressed, tileSrc(S.pressed, frame));
        } else {
            p.drawPixmap(rect, S.normal, tileSrc(S.normal, frame));
            if (hovered) {
                // The hover glow is additive, like the original engine
                // blends its hilite sprite.
                p.save();
                p.setCompositionMode(QPainter::CompositionMode_Plus);
                p.drawPixmap(rect, S.hilite);
                p.restore();
            }
        }
        if (cell.mark == Mark::Flag)
            p.drawPixmap(rect, S.flag);
        else if (cell.mark == Mark::Question)
            p.drawPixmap(rect, S.question);
        if (cell.misflagged)   // loss reveal: the wrong flag gets crossed out
            p.drawPixmap(rect, S.misflagX);
        return;
    }

    // A mine the loss cascade hasn't reached yet still hides under its
    // tile; the pops radiate outward from the one that was hit.
    if (m_boomActive && !m_boomDone && cell.mine
        && m_tick < m_boomStart[idx]) {
        p.drawPixmap(rect, S.normal, tileSrc(S.normal, frame));
        return;
    }

    // Opened floor: the pressed gradient tile, with the shadows cast by
    // still-raised neighbours (and the frame along the field's edge).
    p.drawPixmap(rect, S.pressed, tileSrc(S.pressed, frame));

    const auto casts = [&](int rr, int cc) {
        return !m_board->at(rr, cc).revealed;
    };
    const bool top = (r == 0) || casts(r - 1, c);
    const bool left = (c == 0) || casts(r, c - 1);
    const int t = S.shadowU.height();
    if (top)
        p.drawPixmap(rect.topLeft(), S.shadowU);
    if (left) {
        if (top)   // don't double-darken the shared corner
            p.drawPixmap(rect.left(), rect.top() + t, S.shadowL,
                         0, t, t, cs - t);
        else
            p.drawPixmap(rect.topLeft(), S.shadowL);
    }

    if (cell.mine) {
        // Loss reveal: once this mine's turn comes its crater sits on the
        // floor and the explosion (painted over the grid) plays on top,
        // the fading smoke uncovering the remnant. A flower blooms up from
        // nothing, so it appears only once its animation has finished.
        if (!m_boomActive) {   // shouldn't happen; never leave a bare floor
            p.drawPixmap(rect, S.mine);
            return;
        }
        const bool started = m_boomDone || m_tick >= m_boomStart[idx];
        const bool finished = m_boomDone
                              || m_tick >= m_boomStart[idx] + kTripTicks;
        if (m_gameStyle == Theme::GameStyle::Flowers ? finished : started)
            p.drawPixmap(rect, cell.exploded ? S.craterRed : S.crater);
    } else if (cell.adjacent > 0) {
        const QPixmap &d = S.digits[cell.adjacent - 1];
        p.drawPixmap(rect.left() + (cs - d.width()) / 2,
                     rect.top() + (cs - d.height()) / 2, d);
    }

    // The dissolving tile of a flood ripple, over everything the cell
    // just revealed.
    if (cover > 0.0) {
        p.setOpacity(cover);
        p.drawPixmap(rect, S.normal, tileSrc(S.normal, frame));
        p.setOpacity(1.0);
    }
}

void BoardWidget::paintWinSweep(QPainter &p, const Metrics &m) const
{
    if (!m_winActive || m_winDone)
        return;
    const QPixmap &other = Theme::otherSheet();
    const int k = Theme::kScale;
    p.save();
    p.setClipRect(m.grid);

    // Disarm beams above every mine the scan bar has passed.
    for (int i = 0; i < m_disarmStart.size(); ++i) {
        const int start = m_disarmStart[i];
        if (start < 0 || m_tick < start || m_tick >= start + kDisarmTicks)
            continue;
        const int frame = qMin(Theme::kDisarmFrames - 1,
                               (m_tick - start) * Theme::kDisarmFrames
                                   / kDisarmTicks);
        const QRect src = Theme::disarmFrame(m_boardStyle, frame);
        const int r = i / m_board->cols();
        const int c = i % m_board->cols();
        const QRectF dst(m.grid.left() + c * m.cell
                             + Theme::kDisarmOffset.x() * m.f,
                         m.grid.top() + r * m.cell
                             + Theme::kDisarmOffset.y() * m.f,
                         src.width() * m.f, src.height() * m.f);
        p.drawPixmap(dst, other,
                     QRectF(src.x() * k, src.y() * k, src.width() * k,
                            src.height() * k));
    }

    // The scan bar itself, sweeping from below the field to above it.
    if (m_tick <= kScanTicks) {
        const QRect src = Theme::kScanBar;
        const qreal barH = qreal(m.grid.width()) * src.height() / src.width();
        const qreal y = m.grid.bottom()
                        - (m.grid.height() + barH) * m_tick / kScanTicks;
        p.drawPixmap(QRectF(m.grid.left(), y, m.grid.width(), barH), other,
                     QRectF(src.x() * k, src.y() * k, src.width() * k,
                            src.height() * k));
    }
    p.restore();
}

void BoardWidget::paintEvent(QPaintEvent *)
{
    const Metrics m = metrics();
    QPainter p(this);
    paintBackdrop(p, m);
    if (m.grid.isEmpty())
        return;

    for (int r = 0; r < m_board->rows(); ++r)
        for (int c = 0; c < m_board->cols(); ++c)
            paintCell(p, m, r, c);

    // The loss cascade's explosions, painted over the grid so each
    // fireball can spill across its neighbours' tiles; the craters
    // underneath are the grid pass's business.
    if (m_boomActive && !m_boomDone) {
        const QPixmap &other = Theme::otherSheet();
        const int k = Theme::kScale;
        const QPoint off = Theme::tripOffset(m_gameStyle);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        for (int i = 0; i < m_boomStart.size(); ++i) {
            if (m_boomStart[i] < 0 || m_tick < m_boomStart[i]
                || m_tick >= m_boomStart[i] + kTripTicks)
                continue;
            const int frame = (m_tick - m_boomStart[i]) * Theme::kTripFrames
                              / kTripTicks;
            const QRect src = Theme::tripFrame(m_gameStyle, frame);
            const int r = i / m_board->cols();
            const int c = i % m_board->cols();
            const QRectF dst(m.grid.left() + c * m.cell + off.x() * m.f,
                             m.grid.top() + r * m.cell + off.y() * m.f,
                             src.width() * m.f, src.height() * m.f);
            p.drawPixmap(dst, other,
                         QRectF(src.x() * k, src.y() * k, src.width() * k,
                                src.height() * k));
        }
    }

    paintWinSweep(p, m);

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
        finishWinNow();
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
        if (m_press.x() >= 0 && !m_board->over()) {
            if (!m_board->chord(m_press.y(), m_press.x()))
                emit invalidMove();
        }
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
