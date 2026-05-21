# DS Minesweeper

A Minesweeper clone for Nintendo DS, built with BlocksDS/Wonderful.

Targets: melonDS and DraStic.

## Build

```bash
make clean
make
```

Output ROM:

```text
ds-minesweeper.nds
```

## Controls

- D-pad: move cursor
- A: reveal tile
- B: flag tile
- START: new game
- SELECT: cycle difficulty
- X/Y: change difficulty
- L/R: pan horizontally on large boards

Touch is lower-screen only:

- Tap: reveal
- Long tap: flag
- Drag: pan board

Top-screen smiley and difficulty buttons are visual only; real DS hardware has touch only on the lower screen.

## Project Notes

The game uses a generated custom ARM7 core.

Why:

- Standard BlocksDS/libnds sound helpers froze on DraStic when sound was triggered.
- The custom ARM7 core plays the exact PCM click samples directly.
- ARM9 sends small sound commands to ARM7 through IPCSYNC.
- ARM7 also writes touch state into shared IPC RAM.
- ARM9 reads that shared touch state and applies it to the normal board touch logic.

Generated ARM7 files:

```text
build/custom_arm7/arm7_sound.c
build/custom_arm7/arm7_sound.elf
```

More details are in the docs folder.



## License

This project is released under the MIT License. See `LICENSE`.

## Notice

This is an unofficial Nintendo DS homebrew project. It is not affiliated with or endorsed by Nintendo, Microsoft, or the original Minesweeper developers.

No commercial ROM, copyrighted game asset, Nintendo SDK file, Microsoft artwork, or original Windows Minesweeper asset is included in this repository.
