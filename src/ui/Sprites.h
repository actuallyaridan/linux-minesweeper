#pragma once

// The one piece of artwork still painted by hand: the translucent Bosnian
// flag draped over the minefield on the Bosnia difficulty. (Everything else
// comes from the original Windows 7 sprites, see SpriteSheet.h.)
//
// Header-only and moc-free, like linux-control's Win7Ui.h.

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QtMath>

namespace Sprites {

// The Bosnian flag, stretched across the minefield and painted translucent
// over the tiles. The design is squeezed to the board's shape rather than
// the flag's 1:2 ratio: the hypotenuse runs corner to corner with the yellow
// triangle above it, the blue field below, and the upright stars riding the
// diagonal (the end ones clipped by the frame, as on the real flag's edges).
// Alphas are kept low so the numbers underneath stay readable.
inline void paintBosniaFlag(QPainter &p, const QRectF &r)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(r);
    p.setPen(Qt::NoPen);

    // Warmer and stronger than the blue wash: a faint pure yellow over the
    // blue tiles just turns olive, so the gold needs the extra push (and a
    // little red) to still read as the flag's triangle.
    p.fillRect(r, QColor(0xFF, 0xC8, 0x1E, 120));

    QPainterPath blue;
    blue.moveTo(r.topLeft());
    blue.lineTo(r.bottomRight());
    blue.lineTo(r.bottomLeft());
    blue.closeSubpath();
    // Painted opaque enough to cancel the yellow wash underneath.
    p.fillPath(blue, QColor(0x1A, 0x35, 0x8F, 110));

    const QPointF diag = r.bottomRight() - r.topLeft();
    const qreal len = std::hypot(diag.x(), diag.y());
    const QPointF dir = diag / len;
    const QPointF below(-dir.y(), dir.x());   // perpendicular, into the blue

    const int starCount = 9;                  // seven whole, two clipped
    const qreal radius = len / starCount * 0.42;
    const QColor white(0xFF, 0xFF, 0xFF, 80);
    for (int i = 0; i < starCount; ++i) {
        // Centres sit a full outer radius below the hypotenuse (plus a
        // small gap), so the stars stay entirely on the blue like the
        // real flag, instead of straddling the diagonal.
        const QPointF c = r.topLeft() + diag * ((i + 0.5) / starCount)
                          + below * radius * 1.15;
        QPainterPath star;
        for (int k = 0; k < 5; ++k) {
            const qreal out = -M_PI / 2 + k * 2 * M_PI / 5;
            const qreal in = out + M_PI / 5;
            const QPointF po = c + radius * QPointF(qCos(out), qSin(out));
            const QPointF pi = c + radius * 0.382 * QPointF(qCos(in), qSin(in));
            if (k == 0)
                star.moveTo(po);
            else
                star.lineTo(po);
            star.lineTo(pi);
        }
        star.closeSubpath();
        p.fillPath(star, white);
    }
    p.restore();
}

} // namespace Sprites
