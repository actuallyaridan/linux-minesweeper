#pragma once

#include <QDialog>

// The Help > View Help window, carrying the Windows 7 Help and Support
// article for Minesweeper: how to play, the rules and basics, and the
// hints, ending on the game's own board illustration. Modeless, like the
// original help viewer.
class HelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget *parent = nullptr);
};
