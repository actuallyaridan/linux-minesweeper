#pragma once

#include <QDialog>

class QComboBox;
class QLabel;

// The Game > Statistics dialog: per-difficulty lifetime numbers with a
// difficulty picker, like Windows 7's. Opens on the difficulty being played.
class StatisticsDialog : public QDialog {
    Q_OBJECT
public:
    explicit StatisticsDialog(const QString &currentDifficultyId,
                              QWidget *parent = nullptr);

private:
    void refresh();

    QComboBox *m_combo;
    QLabel *m_values[6];   // played, won, win %, best time, streaks
};
