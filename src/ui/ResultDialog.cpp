#include "ResultDialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ResultDialog::ResultDialog(bool won, int seconds, bool record,
                           const Stats::Data *stats, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(won ? tr("Game Won") : tr("Game Lost"));
    setMinimumWidth(420);   // Windows 7's roomy proportions

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(16, 14, 16, 12);
    v->setSpacing(10);

    auto *message = new QLabel(
        won ? tr("Congratulations, you won the game!")
            : tr("Sorry, you lost this game. Better luck next time!"));
    message->setAlignment(Qt::AlignHCenter);
    v->addWidget(message);

    QString time = tr("Time: %1 seconds").arg(seconds);
    if (won && record)
        time += QLatin1Char('\n') + tr("A new record!");
    v->addWidget(new QLabel(time));

    if (stats) {
        v->addSpacing(8);
        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(40);
        grid->addWidget(new QLabel(tr("Games played: %1").arg(stats->played)),
                        0, 0);
        grid->addWidget(new QLabel(tr("Games won: %1").arg(stats->won)), 1, 0);
        const int pct = stats->played > 0 ? stats->won * 100 / stats->played : 0;
        grid->addWidget(new QLabel(tr("Percentage: %1%").arg(pct)), 0, 1, 2, 1,
                        Qt::AlignVCenter);
        grid->setColumnStretch(2, 1);
        v->addLayout(grid);
    }

    auto *link = new QLabel(QStringLiteral(
        "<a href=\"https://github.com/actuallyaridan/linux-minesweeper"
        "#part-of-the-wsl-windows-alike-software-for-linux-series\">%1</a>")
        .arg(tr("Get More Games Online")));
    link->setOpenExternalLinks(true);
    link->setAlignment(Qt::AlignHCenter);
    v->addWidget(link);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    auto addChoice = [this, row](const QString &text, Choice choice,
                                 bool isDefault = false) {
        auto *b = new QPushButton(text);
        b->setDefault(isDefault);
        connect(b, &QPushButton::clicked, this, [this, choice] {
            m_choice = choice;
            accept();
        });
        row->addWidget(b);
    };
    addChoice(tr("Exit"), Choice::Exit);
    if (!won)
        addChoice(tr("Restart this game"), Choice::Restart);
    addChoice(tr("Play again"), Choice::PlayAgain, true);
    v->addSpacing(4);
    v->addLayout(row);

    // Closing the dialog any other way keeps playing, like Esc did before.
    m_choice = Choice::PlayAgain;
}
