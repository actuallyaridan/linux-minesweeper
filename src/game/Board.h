#pragma once

#include <QObject>
#include <QPoint>
#include <QVector>

// Right-click cycle on a covered cell: none -> flag -> question -> none
// (question is skipped when disabled in Options, like Windows 7).
enum class Mark : quint8 { None, Flag, Question };

struct Cell {
    bool mine = false;
    bool revealed = false;
    bool exploded = false;    // the mine that ended the game (painted on red)
    bool misflagged = false;  // flagged but no mine underneath, shown on loss
    Mark mark = Mark::None;
    quint8 adjacent = 0;
};

// The whole game state and rules, with no widget code: the widgets only read
// cells and call reveal()/toggleMark()/chord(). Mines are placed on the first
// reveal rather than at reset so the first click can never be a mine, which is
// also how Windows 7 does it.
class Board : public QObject {
    Q_OBJECT
public:
    enum class State { Ready, Playing, Won, Lost };

    explicit Board(QObject *parent = nullptr);

    void reset(int rows, int cols, int mines);
    // The dialog's "Restart this game": same minefield, fresh cover. The
    // clock restarts on the next reveal. Since the mines are already placed,
    // the first click is no longer guaranteed safe.
    void restart();
    void reveal(int row, int col);
    void toggleMark(int row, int col);
    // Reveal the covered neighbours of a revealed number whose mines are all
    // flagged (middle-click / both-buttons in the widget). Wrong flags lose.
    void chord(int row, int col);

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    int mineCount() const { return m_mines; }
    int minesRemaining() const { return m_mines - m_flags; }
    State state() const { return m_state; }
    bool over() const { return m_state == State::Won || m_state == State::Lost; }
    const Cell &at(int row, int col) const { return m_cells[row * m_cols + col]; }

    void setQuestionMarksAllowed(bool allowed) { m_questionMarks = allowed; }

signals:
    void boardChanged();
    void minesRemainingChanged(int remaining);
    void gameStarted();           // first cell revealed; the clock starts here
    void gameEnded(bool won);
    // A safe reveal's cells in the order they opened (breadth-first, so a
    // flood ripples outward). x = column, y = row. Size > 1 means a flood.
    void cellsOpened(const QVector<QPoint> &order);
    void markToggled();

private:
    Cell &cell(int row, int col) { return m_cells[row * m_cols + col]; }
    void placeMines(int safeRow, int safeCol);
    void floodReveal(int row, int col, QVector<QPoint> &order);
    void lose();
    void checkWin();
    template <typename F> void forNeighbors(int row, int col, F f) const;

    QVector<Cell> m_cells;
    int m_rows = 0;
    int m_cols = 0;
    int m_mines = 0;
    int m_flags = 0;
    int m_revealed = 0;
    State m_state = State::Ready;
    bool m_minesPlaced = false;
    bool m_questionMarks = true;
};
