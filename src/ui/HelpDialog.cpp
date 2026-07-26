#include "HelpDialog.h"

#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

// The Windows 7 Help and Support styling: white page, blue collapsible
// section headings, blue task links (decorative here) and grey asides.
const char *kHelpHtml = R"(
<style>
  body { background: white; color: #1a1a1a; }
  .link { color: #0066cc; }
  .aside { color: #555555; }
</style>
<body>

<p style="font-size:17pt; color:#333333;">Minesweeper: how to play</p>

<p>Minesweeper is a deceptively simple test of memory and reasoning, and
one of the most popular Windows games of all time. The goal: find the
empty squares and avoid the mines.</p>
<p>Sounds easy, right?</p>

<p style="font-size:11pt; color:#1570c4;"><b>&#9660; To start a game</b></p>
<ol>
  <li><span class="link">&#10140; Click to open Games.</span></li>
  <li>Double-click <b>Minesweeper</b>.<br>
      <span class="aside">(Don't see it? You might need to turn on
      Windows Games. See <span class="link">Where are my
      games?</span>)</span></li>
  <li>Choose a difficulty level: Beginner, Intermediate, or Advanced.</li>
  <li>To start, click a tile.</li>
</ol>

<p style="font-size:11pt; color:#1570c4;"><b>&#9660; To save a game</b></p>
<ul>
  <li>If you need to finish a game later, you can exit the game and then
      click <b>Save</b>. The next time you play, you'll be asked whether
      you want to continue your last game. If so, click <b>Yes</b>.</li>
</ul>

<p style="font-size:11pt; color:#1570c4;"><b>&#9660; To change game options</b></p>
<p>You can adjust the difficulty level, turn animation on and off, and
more.</p>
<ol>
  <li><span class="link">&#10140; Click to open Games.</span></li>
  <li>Double-click <b>Minesweeper</b>.<br>
      <span class="aside">(Don't see it? You might need to turn on
      Windows Games. See <span class="link">Where are my
      games?</span>)</span></li>
  <li>Click the <b>Game</b> menu, and then click <b>Options</b>.</li>
  <li>Make your choices, and then click <b>OK</b>.</li>
</ol>

<p style="font-size:11pt; color:#1570c4;"><b>&#9660; To customize the game's appearance</b></p>
<p>You can change the board color, and whether it conceals mines or
flowers.</p>
<ol>
  <li><span class="link">&#10140; Click to open Games.</span></li>
  <li>Double-click <b>Minesweeper</b>.<br>
      <span class="aside">(Don't see it? You might need to turn on
      Windows Games. See <span class="link">Where are my
      games?</span>)</span></li>
  <li>Click the <b>Game</b> menu, and then click
      <b>Change Appearance</b>.</li>
  <li>Make your choices, and then click <b>OK</b>.</li>
</ol>

<p style="font-size:17pt; color:#333333;">Minesweeper: Rules and basics</p>

<p style="font-size:10pt;"><b>The object</b></p>
<p>Find the empty squares while avoiding the mines. The faster you clear
the board, the better your score.</p>

<p style="font-size:10pt;"><b>The board</b></p>
<p>Minesweeper has three standard boards to choose from, each
progressively more difficult.</p>
<ul>
  <li><b>Beginner:</b> 81 tiles, 10 mines</li>
  <li><b>Intermediate:</b> 256 tiles, 40 mines</li>
  <li><b>Expert:</b> 480 tiles, 99 mines</li>
</ul>
<p>You can also create a custom board by clicking the <b>Game</b> menu,
and then clicking <b>Options</b>. Minesweeper supports boards of up to
720 squares and 668 mines.</p>

<p style="font-size:10pt;"><b>How to play</b></p>
<p>The rules in Minesweeper are simple:</p>
<ul>
  <li>Uncover a mine, and the game ends.</li>
  <li>Uncover an empty square, and you keep playing.</li>
  <li>Uncover a number, and it tells you how many mines lay hidden in
      the eight surrounding squares, information you use to deduce which
      nearby squares are safe to click.</li>
</ul>

<p style="font-size:10pt;"><b>Hints and tips</b></p>
<ul>
  <li><b>Mark the mines.</b> If you suspect a square conceals a mine,
      right-click it. This puts a flag on the square. (If you're not
      sure, right-click again to make it a question mark.)</li>
  <li><b>Study the patterns.</b> If three squares in a row display
      2-3-2, then you know three mines are probably lined up beside that
      row. If a square says 8, every surrounding square is mined.</li>
  <li><b>Explore the unexplored.</b> Not sure where to click next? Try
      clearing some unexplored territory. You're better off clicking in
      the middle of unmarked squares than in an area you suspect is
      mined.</li>
</ul>

<p align="center"><img src=":/assets/helpBoard.png"></p>

</body>
)";

} // namespace

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Minesweeper Help"));
    resize(543, 700);   // the original help window's proportions

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);

    auto *page = new QTextBrowser;
    page->setOpenExternalLinks(false);
    page->setFrameShape(QFrame::NoFrame);
    page->document()->setDocumentMargin(18);
    page->setHtml(QString::fromUtf8(kHelpHtml));
    v->addWidget(page);
}
