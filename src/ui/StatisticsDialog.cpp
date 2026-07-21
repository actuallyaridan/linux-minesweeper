#include "StatisticsDialog.h"

#include "Difficulty.h"
#include "Statistics.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

StatisticsDialog::StatisticsDialog(const QString &currentDifficultyId,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Statistics"));

    auto *v = new QVBoxLayout(this);

    m_combo = new QComboBox;
    m_combo->addItem(tr("Beginner"), Difficulties::Beginner.id);
    m_combo->addItem(tr("Intermediate"), Difficulties::Intermediate.id);
    m_combo->addItem(tr("Advanced"), Difficulties::Advanced.id);
    m_combo->addItem(tr("Bosnia"), Difficulties::Bosnia.id);
    const int idx = m_combo->findData(currentDifficultyId);
    m_combo->setCurrentIndex(qMax(0, idx));   // custom games map to Beginner
    connect(m_combo, &QComboBox::currentIndexChanged,
            this, &StatisticsDialog::refresh);
    v->addWidget(m_combo);

    auto *form = new QFormLayout;
    const char *captions[6] = {
        QT_TR_NOOP("Games played:"),   QT_TR_NOOP("Games won:"),
        QT_TR_NOOP("Win percentage:"), QT_TR_NOOP("Best time:"),
        QT_TR_NOOP("Longest winning streak:"),
        QT_TR_NOOP("Longest losing streak:"),
    };
    for (int i = 0; i < 6; ++i) {
        m_values[i] = new QLabel;
        form->addRow(tr(captions[i]), m_values[i]);
    }
    v->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *reset = buttons->addButton(tr("Reset"),
                                     QDialogButtonBox::DestructiveRole);
    connect(reset, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(
            this, tr("Reset Statistics"),
            tr("Erase all saved statistics for every difficulty?"));
        if (answer == QMessageBox::Yes) {
            Stats::reset();
            refresh();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(buttons);

    refresh();
}

void StatisticsDialog::refresh()
{
    const Stats::Data d = Stats::load(m_combo->currentData().toString());
    m_values[0]->setText(QString::number(d.played));
    m_values[1]->setText(QString::number(d.won));
    m_values[2]->setText(d.played > 0
                             ? QStringLiteral("%1%").arg(d.won * 100 / d.played)
                             : tr("N/A"));
    m_values[3]->setText(d.bestTime > 0
                             ? tr("%1 seconds").arg(d.bestTime)
                             : tr("N/A"));
    m_values[4]->setText(QString::number(d.bestWinStreak));
    m_values[5]->setText(QString::number(d.worstLossStreak));
}
