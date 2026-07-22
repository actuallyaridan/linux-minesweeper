#include <QApplication>
#include <QIcon>
#include "MainWindow.h"
#ifdef MINESWEEPER_DEMO
#include <QMenuBar>
#endif

#ifdef MINESWEEPER_DEMO
#include <QDir>
#include <QTimer>
#include "Board.h"
#include "BoardWidget.h"

// A scripted run for eyeballing the artwork headless (offscreen platform):
// deal a board, open and flag some squares, then lose (or win, with
// MINESWEEPER_DEMO_WIN set), grabbing PNGs of every stage into /tmp.
static void runDemo(MainWindow *w)
{
    auto *board = w->findChild<Board *>();
    auto *widget = w->findChild<BoardWidget *>();
    const bool winDemo = qEnvironmentVariableIsSet("MINESWEEPER_DEMO_WIN");
    const auto shot = [w](const QString &name) {
        w->grab().save(QStringLiteral("/tmp/demo_%1.png").arg(name));
    };
    int t = 0;
    const auto at = [&t](int ms, auto fn) {
        QTimer::singleShot(t += ms, fn);
    };
    if (qEnvironmentVariableIsSet("MINESWEEPER_DEMO_ABOUT")
        || qEnvironmentVariableIsSet("MINESWEEPER_DEMO_APPEAR")) {
        // Grab the modal dialog from inside its own nested exec loop.
        QTimer::singleShot(600, [] {
            if (QWidget *dlg = QApplication::activeModalWidget()) {
                dlg->grab().save("/tmp/demo_dialog.png");
                qInfo("dialog size: %dx%d", dlg->width(), dlg->height());
            }
            QApplication::quit();
        });
        const QString want = qEnvironmentVariableIsSet("MINESWEEPER_DEMO_APPEAR")
                                 ? QStringLiteral("Appearance")
                                 : QStringLiteral("About");
        at(300, [w, want] {
            for (QMenu *menu : w->menuBar()->findChildren<QMenu *>())
                for (QAction *a : menu->actions())
                    if (a->text().contains(want)) a->trigger();
        });
        return;
    }
    at(400, [=] { shot("dealing"); });   // mid deal-in
    at(500, [] {});                      // the rest run past the intro
    if (qEnvironmentVariableIsSet("MINESWEEPER_DEMO_BIG"))
        at(0, [=] { w->resize(w->size() * 2); });
    at(0, [=] { shot("fresh"); });
    at(100, [=] { board->reveal(4, 4); });
    at(100, [=] {
        for (int r = 0; r < board->rows() && board->minesRemaining() > 8; ++r)
            for (int c = 0; c < board->cols(); ++c)
                if (board->at(r, c).mine && board->minesRemaining() > 8)
                    board->toggleMark(r, c);
        shot("opened");
    });
    at(400, [=] {
        if (winDemo) {
            for (int r = 0; r < board->rows(); ++r)
                for (int c = 0; c < board->cols(); ++c)
                    if (!board->at(r, c).mine)
                        board->reveal(r, c);
        } else {
            board->toggleMark(0, 0);   // misflag one safe corner...
            for (int r = 0; r < board->rows(); ++r)
                for (int c = 0; c < board->cols(); ++c)
                    if (board->at(r, c).mine && !board->at(0, 0).mine
                        && board->at(r, c).mark == Mark::None) {
                        board->reveal(r, c);   // ...and step on a mine
                        r = board->rows();
                        break;
                    }
        }
    });
    at(500, [=] { shot(winDemo ? "win_mid" : "boom_mid"); });
    at(700, [=] { shot(winDemo ? "win_late" : "boom_late"); });
    at(2500, [=] { shot(winDemo ? "win_end" : "boom_end"); });
    at(300, [] { QApplication::quit(); });
}
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("minesweeper");
    app.setApplicationName("minesweeper");
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/Minesweeper.ico")));
    MainWindow w;
    w.show();
#ifdef MINESWEEPER_DEMO
    runDemo(&w);
#endif
    return app.exec();
}
