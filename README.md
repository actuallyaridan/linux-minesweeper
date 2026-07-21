<div align="center">

  # linux-minesweeper
  <i>The Windows 7 Minesweeper, on Linux</i>

  <p>
    A faithful recreation of the Windows 7 Minesweeper built with Qt6, using the original game's artwork — the tiles, mines, flags, counters, and window frame are blitted from the sprite sheets extracted by <a href="https://github.com/Lmy0217/Minesweeper">Lmy0217/Minesweeper</a>. Best enjoyed with <a href="https://github.com/aeroshell-desktop/aerothemeplasma">AeroThemePlasma</a>.
  </p>

</div>
<br>

## Building

```bash
sudo pacman -S qt6-base qt6-multimedia cmake
cmake -B build
cmake --build build -j
./build/minesweeper
```

Only Qt6 Widgets and Multimedia (for the sounds) are required, so this should build unchanged on any distro with Qt6 packages.

## Features

- The Windows 7 look, from the Windows 7 sprites: the real tiles with hover glow and press states, the drop shadows raised tiles cast onto opened cells, the real frame backdrop, and the navy time/mine counters with their clock and mine buttons
- The Windows 7 sounds: the new-game deal, the flood-open ripple, the flag click, and the three-stage explosion on a loss (toggleable in Options)
- The Windows 7 animations: every new board deals itself in with tiles popping up in waves from the corners, floods ripple open outward, and losing sets off the full mine cascade — the hit mine detonates first, the rest go off in rings radiating out from it (click to skip)
- The Windows 7 rules: the first square you reveal is never a mine, flags and question marks, chording (middle-click or both buttons on a satisfied number), auto-flagging on a win
- Beginner (9×9, 10), Intermediate (16×16, 40), and Advanced (16×30, 99) presets plus custom boards up to 24×30 with 668 mines
- A **Bosnia** difficulty (24×30, 192 mines): technically beatable, statistically a war crime — played under a faint Bosnian flag painted over the minefield
- Per-difficulty lifetime statistics: games played/won, win percentage, best time, and streaks
- The board scales with the window, like the original
- Win/lose dialogs with "Play again", and your difficulty and options are remembered across launches

## Controls

| Input | Action |
|-------|--------|
| Left-click | Reveal a square |
| Right-click | Flag → question mark → clear |
| Middle-click (or left+right) | Chord: clear the neighbours of a satisfied number |
| <kbd>F2</kbd> | New game |
| <kbd>F4</kbd> | Statistics |
| <kbd>F5</kbd> | Options |

## Part of the WSL (Windows-alike Software for Linux) series

Why don't you also check out the other ones?

- [Linux Device Manager](https://github.com/actuallyaridan/linux-devmgmt)
- [Linux Control Panel](https://github.com/actuallyaridan/linux-control)
- Linux Minesweeper
