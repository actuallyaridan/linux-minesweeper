#pragma once

// The original Windows 7 Minesweeper artwork, as extracted by the
// Lmy0217/Minesweeper project (github.com/Lmy0217/Minesweeper). Two bitmaps:
//
//  Minesweeper.bmp  the cell sprites, an 8 x 26 grid of 18x18 tiles. The
//                   layout was decoded from that project's source: a cell
//                   state 0x0CRR blits sheet column C, row RR. Column 0 is
//                   the covered tile (pressed / normal / hover, with the
//                   question mark and flag baked in), column 1 the opened
//                   floor and numbers, column 2 the misflag cross and the
//                   3px shadow strips, columns 3-7 the mine variants.
//
//  Frame.bmp        the window backdrop (600x351), stretched as a 9-patch
//                   with 30/30/29/32 px edges. The clock button, mine
//                   button and navy counter box are stashed inside its
//                   white centre at (30,30)-(95,80), where the minefield
//                   normally covers them.
//
// Everything is header-only and moc-free, like Win7Ui.h in linux-control.

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QString>

namespace Sheet {

constexpr int kTile = 18;    // native sprite size in the sheet

struct Sprite { int col; int row; };

// Column 0: the covered tile in its three states. Unlike my first hand-paint
// guess, the real pressed tile is *lighter* than the resting one.
constexpr Sprite QuestionDown{0, 0}, Question{0, 1}, QuestionHover{0, 2};
constexpr Sprite BrickDown{0, 3},    Brick{0, 4},    BrickHover{0, 5};
constexpr Sprite FlagDown{0, 6},     Flag{0, 7},     FlagHover{0, 8};

// Column 1: the opened floor; rows 1-8 carry the numbers.
constexpr Sprite Floor{1, 0};
constexpr Sprite Number(int n) { return Sprite{1, n}; }

// The loss reveal: mines surface through their still-raised tiles.
constexpr Sprite Misflag{2, 0};        // crossed-out mine on the floor
constexpr Sprite MineBrick{4, 0};      // an unfound mine
constexpr Sprite RedMineBrick{7, 0};   // the one that went off

// Below each mine variant sit its explosion frames: ignition through
// fireball to the grey burnt-out remnant. Frame 0 is the resting mine.
constexpr int kBoomFrames = 25;
constexpr Sprite MineBoom(int frame) { return Sprite{4, frame}; }
constexpr Sprite RedMineBoom(int frame) { return Sprite{7, frame}; }

// The 9-patch geometry of Frame.bmp.
constexpr int kFrameW = 600, kFrameH = 351;
constexpr int kGapLeft = 30, kGapTop = 30, kGapRight = 29, kGapBottom = 32;

inline const QPixmap &tiles()
{
    static const QPixmap pm(QStringLiteral(":/assets/Minesweeper.bmp"));
    return pm;
}

inline const QPixmap &frame()
{
    static const QPixmap pm(QStringLiteral(":/assets/Frame.bmp"));
    return pm;
}

// The status-strip ornaments stashed in the frame's centre. Their corner
// pixels are the centre's plain white, which reads as little squares over
// the backdrop gradient, so each sprite is masked to its real outline:
// the buttons to circles, the counter box to its rounded rect.
inline QPixmap masked(const QPixmap &pm, const QPainterPath &outline)
{
    QPixmap out(pm.size());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipPath(outline);
    p.drawPixmap(0, 0, pm);
    return out;
}

inline const QPixmap &clockButton()
{
    static const QPixmap pm = [] {
        QPainterPath circle;
        circle.addEllipse(QRectF(0, 0, 25, 25));
        return masked(frame().copy(30, 30, 25, 25), circle);
    }();
    return pm;
}

inline const QPixmap &mineButton()
{
    static const QPixmap pm = [] {
        QPainterPath circle;
        circle.addEllipse(QRectF(0, 0, 25, 25));
        return masked(frame().copy(30, 55, 25, 25), circle);
    }();
    return pm;
}

inline const QPixmap &counterBox()
{
    static const QPixmap pm = [] {
        QPainterPath box;
        // The radius follows the sprite's own corner curve, cutting away
        // exactly the white outside it.
        box.addRoundedRect(QRectF(0, 0, 40, 22), 5, 5);
        return masked(frame().copy(55, 30, 40, 22), box);
    }();
    return pm;
}

// Scale the sheet so one sprite covers `cell` pixels. Each sprite is scaled
// on its own instead of scaling the sheet in one go: the smooth filter would
// otherwise sample across sprite boundaries and bleed neighbours into each
// tile's border pixels.
inline QPixmap scaledTiles(int cell)
{
    const QPixmap &src = tiles();
    const int cols = src.width() / kTile;
    const int rows = src.height() / kTile;
    QPixmap out(cols * cell, rows * cell);
    QPainter p(&out);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            p.drawPixmap(c * cell, r * cell,
                         src.copy(c * kTile, r * kTile, kTile, kTile)
                             .scaled(cell, cell, Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation));
        }
    }
    return out;
}

} // namespace Sheet
