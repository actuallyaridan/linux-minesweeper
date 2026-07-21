#include "OptionsDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// A preset radio with its grey description lines underneath, the way the
// Windows 7 dialog describes each difficulty ("10 mines" / "9 x 9 tile grid").
QWidget *presetRow(QRadioButton *radio, const Difficulty &d)
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 6);
    v->setSpacing(1);
    v->addWidget(radio);
    for (const QString &text : {QObject::tr("%1 mines").arg(d.mines),
                                QObject::tr("%1 x %2 tile grid")
                                    .arg(d.cols).arg(d.rows)}) {
        auto *desc = new QLabel(text);
        desc->setStyleSheet("color: #666666;");
        desc->setIndent(22);
        v->addWidget(desc);
    }
    return w;
}

QCheckBox *stubCheckBox(const QString &text)
{
    auto *box = new QCheckBox(text);
    box->setEnabled(false);
    box->setToolTip(QObject::tr("Not implemented"));
    return box;
}

} // namespace

OptionsDialog::OptionsDialog(const Difficulty &current, bool questionMarks,
                             bool sounds, bool animations, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Options"));
    setMinimumWidth(400);   // Windows 7's roomy proportions

    auto *v = new QVBoxLayout(this);

    auto *group = new QGroupBox(tr("Difficulty"));
    auto *columns = new QHBoxLayout(group);
    columns->setSpacing(24);

    m_beginner = new QRadioButton(tr("&Beginner"));
    m_intermediate = new QRadioButton(tr("&Intermediate"));
    m_advanced = new QRadioButton(tr("&Advanced"));
    m_bosnia = new QRadioButton(tr("Bo&snia"));
    m_custom = new QRadioButton(tr("&Custom"));
    auto *radios = new QButtonGroup(this);
    for (auto *r : {m_beginner, m_intermediate, m_advanced, m_bosnia, m_custom})
        radios->addButton(r);

    auto *left = new QVBoxLayout;
    left->setSpacing(0);
    left->addWidget(presetRow(m_beginner, Difficulties::Beginner));
    left->addWidget(presetRow(m_intermediate, Difficulties::Intermediate));
    left->addWidget(presetRow(m_advanced, Difficulties::Advanced));
    left->addWidget(presetRow(m_bosnia, Difficulties::Bosnia));
    left->addStretch(1);
    columns->addLayout(left);

    // The Windows 7 custom limits, spelled out in the labels like it does.
    auto *right = new QVBoxLayout;
    right->setSpacing(6);
    right->addWidget(m_custom);
    auto *form = new QFormLayout;
    form->setContentsMargins(22, 2, 0, 0);
    m_rows = new QSpinBox;
    m_rows->setRange(9, 24);
    m_cols = new QSpinBox;
    m_cols->setRange(9, 30);
    m_mines = new QSpinBox;
    m_mines->setRange(10, 668);
    form->addRow(tr("Height (9-24):"), m_rows);
    form->addRow(tr("Width (9-30):"), m_cols);
    form->addRow(tr("Mines (10-668):"), m_mines);
    right->addLayout(form);
    right->addStretch(1);
    columns->addLayout(right);
    v->addWidget(group);

    // The Windows 7 dialog pads the checkbox block: a slight indent and
    // breathing room above and below, with normal spacing between rows.
    auto *checks = new QVBoxLayout;
    checks->setContentsMargins(10, 10, 0, 10);
    m_animations = new QCheckBox(tr("&Display animations"));
    m_animations->setChecked(animations);
    checks->addWidget(m_animations);
    m_sounds = new QCheckBox(tr("&Play sounds"));
    m_sounds->setChecked(sounds);
    checks->addWidget(m_sounds);
    checks->addWidget(stubCheckBox(tr("Show tips")));
    checks->addWidget(stubCheckBox(tr("Always continue saved game")));
    checks->addWidget(stubCheckBox(tr("Always save game on exit")));
    m_questionMarks = new QCheckBox(
        tr("Allow &question marks (on double right-click)"));
    m_questionMarks->setChecked(questionMarks);
    checks->addWidget(m_questionMarks);
    v->addLayout(checks);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                         | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(buttons);

    const auto syncSpins = [this] {
        const bool custom = m_custom->isChecked();
        m_rows->setEnabled(custom);
        m_cols->setEnabled(custom);
        m_mines->setEnabled(custom);
    };
    connect(radios, &QButtonGroup::buttonToggled, this, syncSpins);

    if (current.id == Difficulties::Intermediate.id)
        m_intermediate->setChecked(true);
    else if (current.id == Difficulties::Advanced.id)
        m_advanced->setChecked(true);
    else if (current.id == Difficulties::Bosnia.id)
        m_bosnia->setChecked(true);
    else if (current.id == QLatin1String("custom"))
        m_custom->setChecked(true);
    else
        m_beginner->setChecked(true);
    m_rows->setValue(current.id == QLatin1String("custom") ? current.rows : 9);
    m_cols->setValue(current.id == QLatin1String("custom") ? current.cols : 9);
    m_mines->setValue(current.id == QLatin1String("custom") ? current.mines : 10);
    syncSpins();
}

Difficulty OptionsDialog::difficulty() const
{
    if (m_intermediate->isChecked())
        return Difficulties::Intermediate;
    if (m_advanced->isChecked())
        return Difficulties::Advanced;
    if (m_bosnia->isChecked())
        return Difficulties::Bosnia;
    if (m_custom->isChecked()) {
        Difficulty d;
        d.id = QStringLiteral("custom");
        d.rows = m_rows->value();
        d.cols = m_cols->value();
        // Same clamp as Windows 7: the field can never be all mines.
        d.mines = qMin(m_mines->value(), (d.rows - 1) * (d.cols - 1));
        return d;
    }
    return Difficulties::Beginner;
}

bool OptionsDialog::questionMarksAllowed() const
{
    return m_questionMarks->isChecked();
}

bool OptionsDialog::soundsEnabled() const
{
    return m_sounds->isChecked();
}

bool OptionsDialog::animationsEnabled() const
{
    return m_animations->isChecked();
}
