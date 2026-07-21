#include "MainWindow.h"

#include "Board.h"
#include "BoardWidget.h"
#include "OptionsDialog.h"
#include "ResultDialog.h"
#include "Statistics.h"
#include "StatisticsDialog.h"

#include <QApplication>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("Minesweeper"));

    m_board = new Board(this);
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::tick);

    buildMenus();
    buildCentral();

    connect(m_board, &Board::gameStarted, this, &MainWindow::onGameStarted);
    connect(m_board, &Board::gameEnded, this, &MainWindow::onGameEnded);
    connect(m_board, &Board::cellsOpened, this,
            [this](const QVector<QPoint> &order) {
        if (order.size() > 1)   // single reveals are silent, like the original
            m_sounds.flood();
    });
    connect(m_board, &Board::markToggled, this, [this] { m_sounds.flag(); });
    connect(m_boardWidget, &BoardWidget::explosionFinished, this, [this] {
        m_sounds.finishExplosion();
        // A beat to take in the smoking crater before the dialog covers it.
        QTimer::singleShot(250, this, [this] {
            if (m_board->state() == Board::State::Lost)
                showEndDialog(false, false);
        });
    });

    loadSettings();
    newGame();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // Size the window for the loaded difficulty on the first show. Deferred
    // a tick so the menu bar has real geometry: the board widget was built
    // before the board had its dimensions, so the hint the window opened
    // with was computed for an empty field.
    if (!m_shownOnce) {
        m_shownOnce = true;
        QTimer::singleShot(0, this, &MainWindow::resizeToDefault);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_snapping || isMaximized() || isFullScreen())
        return;

    // While a programmatic resize is converging, keep re-asserting it
    // instead of snapping: an async WM may deliver the new width before the
    // new height, and snapping that transient wide-but-short shape would
    // shrink the whole window to match it.
    if (m_pendingSize.isValid()) {
        if (m_pendingDeadline.hasExpired()) {
            m_pendingSize = QSize();   // WM had its say; fall through to snap
        } else {
            if (size() != m_pendingSize) {
                QTimer::singleShot(0, this, [this] {
                    if (m_pendingSize.isValid() && size() != m_pendingSize)
                        resize(m_pendingSize);
                });
            }
            return;
        }
    }

    // Keep the window proportional to the play area: snap whatever size the
    // user dragged to the nearest whole-cell fit. snappedSize() of its own
    // result is itself, so the correcting resize() settles immediately.
    const int chrome = menuBar()->isVisible() ? menuBar()->height() : 0;
    const QSize avail(width(), height() - chrome);
    const QSize target = m_boardWidget->snappedSize(avail);
    if (target.isValid() && target != avail) {
        m_snapping = true;
        resize(target.width(), target.height() + chrome);
        m_snapping = false;
    }
}

void MainWindow::resizeToDefault()
{
    m_boardWidget->updateGeometry();
    const int chrome = menuBar()->isVisible() ? menuBar()->height() : 0;
    const QSize hint = m_boardWidget->sizeHint();
    m_pendingSize = QSize(hint.width(), hint.height() + chrome);
    m_pendingDeadline = QDeadlineTimer(400);
    resize(m_pendingSize);
}

void MainWindow::buildMenus()
{
    QMenu *game = menuBar()->addMenu(tr("&Game"));
    game->addAction(tr("&New Game"), QKeySequence(Qt::Key_F2),
                    this, &MainWindow::newGame);
    game->addSeparator();
    game->addAction(tr("&Statistics"), QKeySequence(Qt::Key_F4),
                    this, &MainWindow::showStatistics);
    game->addAction(tr("&Options"), QKeySequence(Qt::Key_F5),
                    this, &MainWindow::showOptions);
    game->addSeparator();
    game->addAction(tr("E&xit"), this, &MainWindow::close);

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&View Help"), QKeySequence(Qt::Key_F1),
                    this, &MainWindow::showHelp);
    help->addAction(tr("&About Minesweeper"), this, &MainWindow::showAbout);
}

void MainWindow::buildCentral()
{
    // The board widget paints the whole play area itself (the Frame.bmp
    // backdrop, the field, and the counters in the bottom strip), so it is
    // the central widget, just like the original's client area.
    m_boardWidget = new BoardWidget(m_board);
    setCentralWidget(m_boardWidget);
}

void MainWindow::loadSettings()
{
    QSettings s;
    const QString id = s.value("difficulty", Difficulties::Beginner.id).toString();
    if (id == Difficulties::Intermediate.id) {
        m_difficulty = Difficulties::Intermediate;
    } else if (id == Difficulties::Advanced.id) {
        m_difficulty = Difficulties::Advanced;
    } else if (id == Difficulties::Bosnia.id) {
        m_difficulty = Difficulties::Bosnia;
    } else if (id == QLatin1String("custom")) {
        m_difficulty.id = QStringLiteral("custom");
        m_difficulty.rows = qBound(9, s.value("custom/rows", 9).toInt(), 24);
        m_difficulty.cols = qBound(9, s.value("custom/cols", 9).toInt(), 30);
        m_difficulty.mines = qBound(10, s.value("custom/mines", 10).toInt(),
                                    (m_difficulty.rows - 1) * (m_difficulty.cols - 1));
    } else {
        m_difficulty = Difficulties::Beginner;
    }
    m_questionMarks = s.value("questionMarks", true).toBool();
    m_board->setQuestionMarksAllowed(m_questionMarks);
    m_sounds.setEnabled(s.value("sounds", true).toBool());
    m_animations = s.value("animations", true).toBool();
    m_boardWidget->setAnimationsEnabled(m_animations);
}

void MainWindow::newGame()
{
    m_timer->stop();
    m_seconds = 0;
    m_boardWidget->setTimeSeconds(0);
    m_boardWidget->setFlagOverlay(m_difficulty.id == Difficulties::Bosnia.id);
    m_board->reset(m_difficulty.rows, m_difficulty.cols, m_difficulty.mines);
    m_boardWidget->playIntro();
    m_sounds.stopAll();   // a restart cuts a still-running explosion short
    m_sounds.newGame();
}

void MainWindow::restartGame()
{
    m_timer->stop();
    m_seconds = 0;
    m_boardWidget->setTimeSeconds(0);
    m_board->restart();   // same minefield, fresh cover
    m_boardWidget->playIntro();
    m_sounds.stopAll();
    m_sounds.newGame();
}

void MainWindow::onGameStarted()
{
    m_timer->start();
}

void MainWindow::onGameEnded(bool won)
{
    m_timer->stop();
    bool record = false;
    // Custom games are not recorded, matching Windows 7.
    if (m_difficulty.id != QLatin1String("custom"))
        record = Stats::recordGame(m_difficulty.id, won, m_seconds);

    if (won) {
        // Let the auto-planted flags be seen for a beat before the dialog.
        QTimer::singleShot(400, this, [this, record] {
            showEndDialog(true, record);
        });
    } else {
        // The run sample loops once per mine still to blow; the game-lost
        // dialog waits for the board widget's cascade to finish.
        int doomed = 0;
        for (int r = 0; r < m_board->rows(); ++r)
            for (int c = 0; c < m_board->cols(); ++c) {
                const Cell &cell = m_board->at(r, c);
                if (cell.mine && cell.mark != Mark::Flag && !cell.exploded)
                    ++doomed;
            }
        m_sounds.explosion(doomed);
    }
}

void MainWindow::showEndDialog(bool won, bool record)
{
    const bool preset = m_difficulty.id != QLatin1String("custom");
    const Stats::Data stats = preset ? Stats::load(m_difficulty.id)
                                     : Stats::Data();
    ResultDialog dlg(won, m_seconds, record, preset ? &stats : nullptr, this);
    dlg.exec();
    switch (dlg.choice()) {
    case ResultDialog::Choice::Exit:
        close();
        break;
    case ResultDialog::Choice::Restart:
        restartGame();
        break;
    case ResultDialog::Choice::PlayAgain:
        newGame();
        break;
    }
}

void MainWindow::tick()
{
    if (m_seconds < 999)   // classic three-digit cap
        m_boardWidget->setTimeSeconds(++m_seconds);
}

void MainWindow::showOptions()
{
    OptionsDialog dlg(m_difficulty, m_questionMarks, m_sounds.enabled(),
                      m_animations, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_questionMarks = dlg.questionMarksAllowed();
    m_board->setQuestionMarksAllowed(m_questionMarks);
    m_sounds.setEnabled(dlg.soundsEnabled());
    m_animations = dlg.animationsEnabled();
    m_boardWidget->setAnimationsEnabled(m_animations);
    const Difficulty d = dlg.difficulty();

    QSettings s;
    s.setValue("questionMarks", m_questionMarks);
    s.setValue("sounds", m_sounds.enabled());
    s.setValue("animations", m_animations);
    s.setValue("difficulty", d.id);
    if (d.id == QLatin1String("custom")) {
        s.setValue("custom/rows", d.rows);
        s.setValue("custom/cols", d.cols);
        s.setValue("custom/mines", d.mines);
    }

    const bool dimsChanged =
        d.rows != m_difficulty.rows || d.cols != m_difficulty.cols;
    m_difficulty = d;
    newGame();
    // A new board shape gets a fresh window at the default zoom, like the
    // original re-fitting itself to the new field.
    if (dimsChanged)
        resizeToDefault();
}

void MainWindow::showStatistics()
{
    StatisticsDialog dlg(m_difficulty.id, this);
    dlg.exec();
}

void MainWindow::showHelp()
{
    QMessageBox::information(
        this, tr("Minesweeper Help"),
        tr("The goal of Minesweeper is to uncover every square that does not "
           "hide a mine.\n\n"
           "• Left-click a square to reveal it. The first square you "
           "reveal is never a mine.\n"
           "• Right-click to flag a square you think is mined; "
           "right-click again for a question mark.\n"
           "• A number shows how many mines touch that square.\n"
           "• Once a number has all its mines flagged, click it with the "
           "middle button (or both buttons) to clear its remaining "
           "neighbours.\n\n"
           "Clear the whole field without setting off a mine to win. Good "
           "luck!"));
}

void MainWindow::showAbout()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("About Minesweeper"));
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dlg.setFixedWidth(340);

    auto *iconLabel = new QLabel;
    iconLabel->setFixedSize(64, 64);
    iconLabel->setPixmap(windowIcon().pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);

    auto *nameLabel = new QLabel(tr("Minesweeper"));
    auto *companyLabel = new QLabel(QStringLiteral("@actuallyaridan"));
    auto *versionLabel = new QLabel(tr("Version: 1.0.0"));

    auto *infoLayout = new QVBoxLayout;
    infoLayout->addWidget(nameLabel);
    infoLayout->addWidget(companyLabel);
    infoLayout->addWidget(versionLabel);
    infoLayout->addStretch();

    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(iconLabel);
    topLayout->addSpacing(8);
    topLayout->addLayout(infoLayout);
    topLayout->addStretch();

    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);

    auto *descLabel = new QLabel(
        tr("Clear the minefield without detonating any of the hidden mines. "
           "Left-click a square to uncover it, right-click to flag a mine."));
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    descLabel->setContentsMargins(4, 4, 4, 4);

    auto *creditsLabel = new QLabel(
        tr("Recreated in Linux with Qt6, using the original Windows 7 artwork "
           "and sounds (extracted by the Lmy0217/Minesweeper project). Best "
           "enjoyed with AeroThemePlasma. Any Microsoft branding is used "
           "solely for referential use only, and does not aim to usurp "
           "copyrights from Microsoft."));
    creditsLabel->setWordWrap(true);
    creditsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    creditsLabel->setContentsMargins(4, 4, 4, 4);

    auto *okBtn = new QPushButton(tr("OK"));
    okBtn->setFixedWidth(80);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);

    auto *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(sep);
    mainLayout->addWidget(descLabel);
    mainLayout->addWidget(creditsLabel);
    mainLayout->addSpacing(4);
    mainLayout->addLayout(btnLayout);

    dlg.layout()->setSizeConstraint(QLayout::SetFixedSize);
    dlg.exec();
}
