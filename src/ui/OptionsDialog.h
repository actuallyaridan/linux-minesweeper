#pragma once

#include <QDialog>
#include "Difficulty.h"

class QCheckBox;
class QRadioButton;
class QSpinBox;

// The Game > Options dialog, laid out like Windows 7's: the Difficulty group
// with presets on the left and the custom board on the right, then the
// option checkboxes. Accepting always starts a new game.
//
// The radios share one QButtonGroup: each sits in its own container widget
// for layout, and QRadioButton auto-exclusivity only spans siblings.
class OptionsDialog : public QDialog {
    Q_OBJECT
public:
    OptionsDialog(const Difficulty &current, bool questionMarks, bool sounds,
                  bool animations, QWidget *parent = nullptr);

    Difficulty difficulty() const;
    bool questionMarksAllowed() const;
    bool soundsEnabled() const;
    bool animationsEnabled() const;

private:
    QRadioButton *m_beginner;
    QRadioButton *m_intermediate;
    QRadioButton *m_advanced;
    QRadioButton *m_bosnia;
    QRadioButton *m_custom;
    QSpinBox *m_rows;
    QSpinBox *m_cols;
    QSpinBox *m_mines;
    QCheckBox *m_animations;
    QCheckBox *m_sounds;
    QCheckBox *m_questionMarks;
};
