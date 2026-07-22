#pragma once

// The Windows 7 Minesweeper window backdrop (as extracted by the
// Lmy0217/Minesweeper project):
//
//  Frame.bmp   600x351, stretched as a 9-patch with 30/30/29/32 px edges.
//
// The cell sprites, counters and animations come from the game's own
// sheets nowadays, see Theme.h. Header-only and moc-free.

#include <QPixmap>
#include <QString>

namespace Sheet {

constexpr int kTile = 18;    // native sprite size, the 9-patch scales to it

// The 9-patch geometry of Frame.bmp.
constexpr int kFrameW = 600, kFrameH = 351;
constexpr int kGapLeft = 30, kGapTop = 30, kGapRight = 29, kGapBottom = 32;

inline const QPixmap &frame()
{
    static const QPixmap pm(QStringLiteral(":/assets/Frame.bmp"));
    return pm;
}

} // namespace Sheet
