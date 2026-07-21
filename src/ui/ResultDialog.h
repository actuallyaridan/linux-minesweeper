#pragma once

#include <QDialog>
#include "Statistics.h"

// The Windows 7 end-of-game dialog: the verdict, the time, the lifetime
// stats block with its win percentage, the "Get More Games Online" link,
// and the Exit / Restart this game / Play again button row (no restart
// after a win, since that board is already solved).
class ResultDialog : public QDialog {
    Q_OBJECT
public:
    enum class Choice { PlayAgain, Restart, Exit };

    // `stats` may be null (custom games aren't recorded, as in Windows 7);
    // the stats block is left out then.
    ResultDialog(bool won, int seconds, bool record, const Stats::Data *stats,
                 QWidget *parent = nullptr);

    Choice choice() const { return m_choice; }

private:
    Choice m_choice = Choice::PlayAgain;
};
