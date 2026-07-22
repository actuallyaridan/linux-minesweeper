#pragma once

// The Windows 7 Minesweeper artwork proper, extracted from the game's own
// resources (see assets/more-resources/ and build_sheets.py). Three kinds
// of sheet, all stored at 2x resolution:
//
//  blueSheet / greenSheet   one per board style. The 720-frame gradient
//                           blocks (normal and pressed tiles: one uniquely
//                           tinted tile per cell of the maximum 30x24
//                           board), the flag / question mark / hover hilite
//                           / misflag X overlays, the two shadow strips,
//                           the digits 1-8 and the counter panel.
//
//  minesweeperSheet /       one per game style: the mine (or flower) that
//  flowerGardenSheet        hides under the tiles, plus the 25x25 icon
//                           shown next to the mines-remaining counter.
//
//  otherSheet               shared: the 23-frame mine-trip (loss) and
//                           38-frame disarm-beam (win) animations, the win
//                           scan bar and the clock icon.
//
// All rects here are in the logical 1x coordinates of the original UI
// XMLs (UI/*.xml in more-resources); multiply by kScale to address the
// stored pixels. Header-only and moc-free, like the rest of the ui layer.

#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QString>

namespace Theme {

enum class BoardStyle { Blue, Green };
enum class GameStyle { Mines, Flowers };

constexpr int kTile = 18;     // logical tile size; sprites are drawn for it
constexpr int kScale = 2;     // the sheets are stored at twice logical size

inline const QPixmap &boardSheet(BoardStyle s)
{
    static const QPixmap blue(QStringLiteral(":/assets/blueSheet.png"));
    static const QPixmap green(QStringLiteral(":/assets/greenSheet.png"));
    return s == BoardStyle::Green ? green : blue;
}

inline const QPixmap &gameSheet(GameStyle g)
{
    static const QPixmap mines(QStringLiteral(":/assets/minesweeperSheet.png"));
    static const QPixmap flowers(
        QStringLiteral(":/assets/flowerGardenSheet.png"));
    return g == GameStyle::Flowers ? flowers : mines;
}

inline const QPixmap &otherSheet()
{
    static const QPixmap pm(QStringLiteral(":/assets/otherSheet.png"));
    return pm;
}

// ---- board sheet ---------------------------------------------------------

// The tile gradient: 720 frames laid out 25 per sheet row, one frame per
// cell of the maximum 30x24 board, running light (top left) to dark
// (bottom right). Smaller boards sample the full gradient proportionally,
// so every board sweeps the same range of colours as the original.
constexpr int kGradCols = 30, kGradRows = 24;

inline int gradientFrame(int row, int col, int rows, int cols)
{
    const int r = rows > 1 ? (row * (kGradRows - 1) + (rows - 1) / 2)
                                 / (rows - 1)
                           : 0;
    const int c = cols > 1 ? (col * (kGradCols - 1) + (cols - 1) / 2)
                                 / (cols - 1)
                           : 0;
    return r * kGradCols + c;
}

inline QRect normalTile(int frame)
{
    return {kTile * (frame % 25), kTile * (frame / 25), kTile, kTile};
}

inline QRect pressedTile(int frame)
{
    return normalTile(frame).translated(500, 0);
}

// Overlays composited onto the tiles. The hilite is additive: the original
// engine adds it onto the hovered tile, which is what makes hover glow.
constexpr QRect kFlag{400, 560, 18, 18};
constexpr QRect kQuestion{420, 560, 18, 18};
constexpr QRect kHilite{440, 560, 18, 18};
constexpr QRect kMisflagX{460, 560, 18, 18};

// A raised tile's shadow onto the opened floor below and right of it.
constexpr QRect kShadowU{480, 560, 18, 3};
constexpr QRect kShadowL{1012, 0, 3, 18};

inline QRect digit(int n)   // 1..8
{
    return {1000, 13 * (n - 1), 10, 13};
}

constexpr QRect kCounterPanel{0, 580, 40, 21};

// ---- game sheet ----------------------------------------------------------

constexpr QRect kMine{27, 0, 18, 18};        // under the tiles
constexpr QRect kPanelMine{0, 0, 25, 25};    // next to the mines counter

// ---- other sheet ---------------------------------------------------------

constexpr QRect kClock{905, 425, 25, 25};    // next to the time counter

// The loss ("mine trip" / bad guess) animation, 23 frames over one second
// per animationMineSweeper_MineTrip.xml: fireball into smoke for the
// mines, a bloom for the flowers. Frames overhang the tile; the offset
// comes from the game styles' MineExplodeX/YOffset.
constexpr int kTripFrames = 23;

// Multi-frame sprites on this sheet are packed with a 2px gutter, so the
// frame pitch is the frame size + 2 (measured: cutting at the bare frame
// size made the explosion drift 2px per frame, the "shaking"). The
// offsets confirm it: green disarm at x=364 (7 frames x 52), flowers at
// x=905 (728 + 3 x 59), the scan bar at y=912 (6 disarm rows x 152).
inline QRect tripFrame(GameStyle g, int frame)
{
    if (g == GameStyle::Flowers)
        return {905 + 38 * (frame % 3), 38 * (frame / 3), 36, 36};
    return {728 + 59 * (frame % 3), 59 * (frame / 3), 57, 57};
}

inline QPoint tripOffset(GameStyle g)
{
    return g == GameStyle::Flowers ? QPoint(-9, -9) : QPoint(-18, -18);
}

// The win ("disarm") animation, 38 frames over two seconds per
// animationMinesweeper_MineDisarm.xml: light beams shine down on the
// mine's tile, bleach it white and burn it away. The frame paints its own
// copy of the tile, sitting at (17,69) within the 50x150 sprite (measured
// from frame 0's opaque pixels), so the beam spills both above and below.
constexpr int kDisarmFrames = 38;
constexpr int kDisarmW = 50, kDisarmH = 150;

inline QRect disarmFrame(BoardStyle s, int frame)
{
    const int x0 = s == BoardStyle::Green ? 364 : 0;
    return {x0 + (kDisarmW + 2) * (frame % 7), (kDisarmH + 2) * (frame / 7),
            kDisarmW, kDisarmH};
}

constexpr QPoint kDisarmOffset{-18, -69};

// The scan bar that sweeps up the board while the mines disarm, stretched
// to the field's width (3 seconds bottom-to-top per its animation XML).
constexpr QRect kScanBar{0, 912, 275, 62};

} // namespace Theme
