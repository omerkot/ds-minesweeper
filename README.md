# DS Minesweeper

<img width="256" height="378" alt="Image" src="https://github.com/user-attachments/assets/54d374ec-f6fa-4f89-b094-6eb3088de1ec" />

A Minesweeper clone for Nintendo DS, built with BlocksDS/Wonderful.

The project targets both melonDS and DraStic. It uses a custom generated ARM7 core for DraStic-compatible sound while keeping lower-screen touch input working through shared IPC RAM.

## Features

- Beginner, Intermediate, and Expert board sizes
- D-pad cursor controls
- Lower-screen touch controls for board reveal, dragging, and long-tap flagging
- Tap-to-restart from the WIN/LOSE popup
- Custom click sounds for movement, flagging, and revealing
- DraStic-compatible custom ARM7 sound path
- Shared-memory touch path for DraStic compatibility
- Custom 256x192 upper-screen artwork
- Code-rendered dynamic HUD text for mine count, flag count, and difficulty
- Double-buffered software rendering for both DS screens

## Controls

### Buttons

- D-pad: move cursor
- A: reveal selected tile
- B: flag selected tile
- START: new game
- SELECT: cycle difficulty
- L/R: pan horizontally on large boards

### Touch

Touch is available only on the lower DS screen.

- Tap a tile: reveal
- Long tap a tile: flag
- Drag: pan the board
- Tap the WIN/LOSE popup after a finished game: start a new game

The top-screen HUD is visual only. It cannot be touched on real Nintendo DS hardware because the touch panel exists only on the lower screen.

## Build

From the project root:

```bash
make clean
make
```

The output ROM is:

```text
ds-minesweeper.nds
```

The Makefile generates these files at build time:

```text
source/generated_top_screen_bg.h
build/custom_arm7/arm7_sound.c
build/custom_arm7/arm7_sound.elf
```

Generated files and ROM outputs are intentionally ignored by Git.

## Project Layout

```text
.
├── Makefile
├── README.md
├── LICENSE
├── NOTICE.md
├── assets/
│   └── top_screen_256x192.png
├── source/
│   └── main.cpp
└── docs/
    ├── ARCHITECTURE.md
    ├── COMPATIBILITY.md
    └── TROUBLESHOOTING.md
```

## Assets

The upper screen uses a clean, already-scaled Nintendo DS bitmap asset:

```text
assets/top_screen_256x192.png
```

The build converts this PNG into a generated RGB555 C header:

```text
source/generated_top_screen_bg.h
```

The mine counter, flag counter, and difficulty text are drawn by code over the background so they can update dynamically.

## Why There Is a Custom ARM7 Core

The normal BlocksDS/libnds sound helpers and Maxmod paths caused DraStic to freeze when starting sound. The final solution keeps the game on BlocksDS but uses a generated custom ARM7 core for audio.

ARM9 sends a tiny command through IPCSYNC, and ARM7 plays the exact PCM samples directly through DS sound hardware.

Touch input also needs ARM7 support. Since the custom ARM7 core bypasses normal system FIFO touch delivery, it writes touch state to a small shared IPC RAM area. ARM9 reads that shared state and feeds it into the existing lower-screen board touch logic.

See `docs/ARCHITECTURE.md` and `docs/COMPATIBILITY.md` for details.

## License

This project is released under the MIT License. See `LICENSE`.

## Notice

This is an unofficial Nintendo DS homebrew project. It is not affiliated with or endorsed by Nintendo, Microsoft, or the original Minesweeper developers.

No commercial ROM, copyrighted game asset, Nintendo SDK file, Microsoft artwork, or original Windows Minesweeper asset is included in this repository.
