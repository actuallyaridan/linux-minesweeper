#include "AppearanceDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <functional>

namespace {

// The Windows 7 selection colours: a soft blue frame around the chosen
// thumbnail and a saturated blue pill behind its caption.
const QColor kFrameLine(0x3C, 0x7F, 0xB1);
const QColor kFrameFill(0xDC, 0xEC, 0xFB);
const QColor kCaptionFill(0x2E, 0x8B, 0xE0);

constexpr int kThumb = 90;    // thumbnails are shown this tall
constexpr int kFramePad = 4;  // gap between the image and its selection frame
constexpr int kGap = 8;       // between the image and its caption

// A clickable thumbnail: the game's artwork with a caption beneath, drawn
// with the Win7 selection frame/highlight when chosen. Header-free (no
// signals), so it needs no moc: the dialog wires a plain callback.
class ThumbTile : public QWidget {
public:
    ThumbTile(const QString &image, const QString &caption, QWidget *parent)
        : QWidget(parent), m_caption(caption)
    {
        m_pixmap = QPixmap(image).scaledToHeight(kThumb,
                                                 Qt::SmoothTransformation);
        setCursor(Qt::PointingHandCursor);
    }

    void setSelected(bool on)
    {
        if (m_selected != on) {
            m_selected = on;
            update();
        }
    }

    std::function<void()> clicked;

    QSize sizeHint() const override
    {
        const int capW = fontMetrics().horizontalAdvance(m_caption) + 16;
        const int w = qMax(m_pixmap.width() + 2 * kFramePad, capW) + 2 * kPad;
        const int h = kThumb + 2 * kFramePad + kGap
                      + fontMetrics().height() + 4 + kPad;
        return QSize(w, h);
    }

protected:
    void mousePressEvent(QMouseEvent *) override
    {
        if (clicked)
            clicked();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // The image, centred at the top with room for its frame.
        const int imgX = (width() - m_pixmap.width()) / 2;
        const int imgY = kPad + kFramePad;
        const QRect img(imgX, imgY, m_pixmap.width(), m_pixmap.height());

        if (m_selected) {
            const QRect frame = img.adjusted(-kFramePad, -kFramePad,
                                             kFramePad, kFramePad);
            p.setPen(kFrameLine);
            p.setBrush(kFrameFill);
            p.drawRoundedRect(frame, 3, 3);
        }
        p.drawPixmap(img.topLeft(), m_pixmap);

        // The caption, on a blue highlight pill when selected.
        const QFontMetrics fm = fontMetrics();
        const int textW = fm.horizontalAdvance(m_caption);
        const int textY = img.bottom() + kGap;
        const QRect pill((width() - textW) / 2 - 6, textY,
                         textW + 12, fm.height() + 4);
        if (m_selected) {
            p.setPen(Qt::NoPen);
            p.setBrush(kCaptionFill);
            p.drawRoundedRect(pill, 3, 3);
            p.setPen(Qt::white);
        } else {
            p.setPen(palette().color(QPalette::WindowText));
        }
        p.drawText(pill, Qt::AlignCenter, m_caption);
    }

private:
    static constexpr int kPad = 6;   // outer breathing room
    QPixmap m_pixmap;
    QString m_caption;
    bool m_selected = false;
};

} // namespace

AppearanceDialog::AppearanceDialog(Theme::BoardStyle board,
                                   Theme::GameStyle game, QWidget *parent)
    : QDialog(parent), m_board(board), m_game(game)
{
    setWindowTitle(tr("Change Appearance"));
    setMinimumWidth(440);   // the original's roomy proportions

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(12, 12, 12, 12);
    v->setSpacing(10);

    // A group of thumbnails where exactly one is selected. `onPick` reports
    // the index chosen; the tiles keep their own selected/unselected paint
    // in step.
    const auto group = [&](const QString &title,
                           const QVector<QPair<QString, QString>> &items,
                           int current, std::function<void(int)> onPick) {
        auto *box = new QGroupBox(title);
        auto *outer = new QVBoxLayout(box);
        outer->setContentsMargins(12, 6, 12, 12);
        auto *row = new QHBoxLayout;
        row->setSpacing(16);

        auto tiles = std::make_shared<QVector<ThumbTile *>>();
        for (int i = 0; i < items.size(); ++i) {
            auto *tile = new ThumbTile(items[i].first, items[i].second, box);
            tile->setSelected(i == current);
            tile->clicked = [tiles, i, onPick] {
                for (int j = 0; j < tiles->size(); ++j)
                    (*tiles)[j]->setSelected(j == i);
                onPick(i);
            };
            tiles->append(tile);
            row->addWidget(tile, 0, Qt::AlignTop);
        }
        row->addStretch(1);
        outer->addLayout(row);
        outer->addStretch(1);
        v->addWidget(box);
    };

    group(tr("Select Game Style"),
          {{QStringLiteral(":/assets/thumbMines.png"), tr("Minesweeper")},
           {QStringLiteral(":/assets/thumbFlowers.png"), tr("Flower Garden")}},
          game == Theme::GameStyle::Flowers ? 1 : 0,
          [this](int i) {
              m_game = i == 1 ? Theme::GameStyle::Flowers
                              : Theme::GameStyle::Mines;
          });

    group(tr("Select Board"),
          {{QStringLiteral(":/assets/thumbBlue.png"), tr("Silver and Blue")},
           {QStringLiteral(":/assets/thumbGreen.png"), tr("Green")}},
          board == Theme::BoardStyle::Green ? 1 : 0,
          [this](int i) {
              m_board = i == 1 ? Theme::BoardStyle::Green
                               : Theme::BoardStyle::Blue;
          });

    // The original's randomize option; not wired up, shown disabled like
    // the other unimplemented toggles in Options.
    auto *randomize =
        new QCheckBox(tr("Randomly choose game style and board"));
    randomize->setEnabled(false);
    randomize->setToolTip(tr("Not implemented"));
    v->addWidget(randomize);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                         | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(buttons);
}
