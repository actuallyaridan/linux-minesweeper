#include "Board.h"

#include <QPoint>
#include <QRandomGenerator>

Board::Board(QObject *parent) : QObject(parent) {}

void Board::reset(int rows, int cols, int mines)
{
    m_rows = rows;
    m_cols = cols;
    // At least one free cell must exist so placeMines() can honour the
    // first-click guarantee (and terminate).
    m_mines = qBound(1, mines, rows * cols - 1);
    m_cells.fill(Cell(), rows * cols);
    m_flags = 0;
    m_revealed = 0;
    m_state = State::Ready;
    m_minesPlaced = false;
    emit minesRemainingChanged(minesRemaining());
    emit boardChanged();
}

void Board::restart()
{
    if (!m_minesPlaced)   // never started: nothing to replay
        return;
    for (Cell &c : m_cells) {
        c.revealed = false;
        c.exploded = false;
        c.misflagged = false;
        c.mark = Mark::None;
    }
    m_flags = 0;
    m_revealed = 0;
    m_state = State::Ready;
    emit minesRemainingChanged(minesRemaining());
    emit boardChanged();
}

template <typename F>
void Board::forNeighbors(int row, int col, F f) const
{
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0)
                continue;
            const int r = row + dr;
            const int c = col + dc;
            if (r >= 0 && r < m_rows && c >= 0 && c < m_cols)
                f(r, c);
        }
    }
}

void Board::placeMines(int safeRow, int safeCol)
{
    const int safe = safeRow * m_cols + safeCol;
    int placed = 0;
    auto *rng = QRandomGenerator::global();
    while (placed < m_mines) {
        const int idx = rng->bounded(m_rows * m_cols);
        if (idx == safe || m_cells[idx].mine)
            continue;
        m_cells[idx].mine = true;
        ++placed;
    }
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            int count = 0;
            forNeighbors(r, c, [&](int nr, int nc) {
                if (at(nr, nc).mine)
                    ++count;
            });
            cell(r, c).adjacent = quint8(count);
        }
    }
}

void Board::reveal(int row, int col)
{
    if (over())
        return;
    Cell &c = cell(row, col);
    if (c.revealed || c.mark == Mark::Flag)
        return;

    if (m_state == State::Ready) {
        if (!m_minesPlaced) {
            placeMines(row, col);
            m_minesPlaced = true;
        }
        m_state = State::Playing;
        emit gameStarted();
    }

    if (c.mine) {
        c.exploded = true;
        lose();
    } else {
        QVector<QPoint> order;
        floodReveal(row, col, order);
        emit cellsOpened(order);
        checkWin();
    }
    emit boardChanged();
}

void Board::floodReveal(int row, int col, QVector<QPoint> &order)
{
    // Iterative flood so an Advanced-sized empty area can't overflow the
    // stack; breadth-first so `order` ripples outward from the click, which
    // is what the reveal animation replays. Flags are respected (never
    // auto-revealed); question marks are cleared when their cell opens.
    QVector<QPoint> pending{QPoint(col, row)};
    while (!pending.isEmpty()) {
        const QPoint pt = pending.takeFirst();
        Cell &c = cell(pt.y(), pt.x());
        if (c.revealed || c.mark == Mark::Flag)
            continue;
        c.revealed = true;
        c.mark = Mark::None;
        ++m_revealed;
        order.append(pt);
        if (c.adjacent == 0) {
            forNeighbors(pt.y(), pt.x(), [&](int r, int cc) {
                if (!at(r, cc).revealed)
                    pending.append(QPoint(cc, r));
            });
        }
    }
}

void Board::toggleMark(int row, int col)
{
    if (over())
        return;
    Cell &c = cell(row, col);
    if (c.revealed)
        return;
    switch (c.mark) {
    case Mark::None:
        c.mark = Mark::Flag;
        ++m_flags;
        break;
    case Mark::Flag:
        --m_flags;
        c.mark = m_questionMarks ? Mark::Question : Mark::None;
        break;
    case Mark::Question:
        c.mark = Mark::None;
        break;
    }
    emit markToggled();
    emit minesRemainingChanged(minesRemaining());
    emit boardChanged();
}

bool Board::chord(int row, int col)
{
    if (m_state != State::Playing)
        return false;
    const Cell &c = at(row, col);
    if (!c.revealed || c.adjacent == 0)
        return false;

    int flags = 0;
    forNeighbors(row, col, [&](int r, int cc) {
        if (at(r, cc).mark == Mark::Flag)
            ++flags;
    });
    if (flags != c.adjacent)
        return false;

    // A wrong flag means an unflagged neighbour is a mine: every such mine
    // goes off at once, then the loss reveal shows them all.
    bool exploded = false;
    forNeighbors(row, col, [&](int r, int cc) {
        Cell &n = cell(r, cc);
        if (!n.revealed && n.mark != Mark::Flag && n.mine) {
            n.exploded = true;
            exploded = true;
        }
    });
    if (exploded) {
        lose();
    } else {
        QVector<QPoint> order;
        forNeighbors(row, col, [&](int r, int cc) {
            const Cell &n = at(r, cc);
            if (!n.revealed && n.mark != Mark::Flag)
                floodReveal(r, cc, order);
        });
        emit cellsOpened(order);
        checkWin();
    }
    emit boardChanged();
    return true;
}

void Board::lose()
{
    m_state = State::Lost;
    for (Cell &c : m_cells) {
        if (c.mine && c.mark != Mark::Flag)
            c.revealed = true;          // show every mine in place
        if (!c.mine && c.mark == Mark::Flag)
            c.misflagged = true;        // flag stays, with a red cross over it
    }
    emit gameEnded(false);
}

void Board::checkWin()
{
    if (m_revealed != m_rows * m_cols - m_mines)
        return;
    m_state = State::Won;
    // Windows 7 plants the remaining flags itself and zeroes the counter.
    for (Cell &c : m_cells) {
        if (c.mine)
            c.mark = Mark::Flag;
    }
    m_flags = m_mines;
    emit minesRemainingChanged(0);
    emit gameEnded(true);
}
