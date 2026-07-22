#pragma once

#include <QDialog>
#include "Theme.h"

// The Game > Change Appearance dialog, laid out like Windows 7's: a
// "Select Game Style" group and a "Select Board" group, each holding
// clickable thumbnails of the original game's own artwork. Clicking a
// thumbnail selects it (a blue frame and a highlighted caption), the way
// the real dialog works.
class AppearanceDialog : public QDialog {
    Q_OBJECT
public:
    AppearanceDialog(Theme::BoardStyle board, Theme::GameStyle game,
                     QWidget *parent = nullptr);

    Theme::BoardStyle boardStyle() const { return m_board; }
    Theme::GameStyle gameStyle() const { return m_game; }

private:
    Theme::BoardStyle m_board;
    Theme::GameStyle m_game;
};
