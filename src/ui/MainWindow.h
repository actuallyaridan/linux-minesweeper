#pragma once

#include <QDeadlineTimer>
#include <QMainWindow>
#include "Difficulty.h"
#include "Sounds.h"

class QTimer;
class Board;
class BoardWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void newGame();
    void restartGame();
    void showOptions();
    void showStatistics();
    void showHelp();
    void showAbout();
    void onGameStarted();
    void onGameEnded(bool won);
    void tick();

private:
    void buildMenus();
    void buildCentral();
    void loadSettings();
    void showEndDialog(bool won, bool record);
    void resizeToDefault();

    Board *m_board;
    BoardWidget *m_boardWidget;
    QTimer *m_timer;
    Sounds m_sounds;
    int m_seconds = 0;
    Difficulty m_difficulty = Difficulties::Beginner;
    bool m_questionMarks = true;
    bool m_animations = true;
    bool m_snapping = false;   // guards the proportional-resize re-entry
    bool m_shownOnce = false;
    // A programmatic resize in flight: re-asserted until the window manager
    // has applied it (or the deadline passes), so an async WM applying width
    // and height separately can't bait the proportional snap into shrinking
    // the window to the transient shape.
    QSize m_pendingSize;
    QDeadlineTimer m_pendingDeadline;
};
