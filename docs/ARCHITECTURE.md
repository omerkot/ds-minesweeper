# Architecture

## Overview

Game logic and rendering run on ARM9 in:

```text
source/main.cpp
```

The Makefile generates two important build-time files:

```text
source/generated_top_screen_bg.h
build/custom_arm7/arm7_sound.c
```

`generated_top_screen_bg.h` contains the RGB555 upper-screen background converted from `assets/top_screen_256x192.png`.

`arm7_sound.c` is a custom ARM7 core generated from the PCM sample arrays in `source/main.cpp`.

## ARM9 Responsibilities

ARM9 handles:

- board generation
- game state
- rendering both screens into software back buffers
- copying buffers to VRAM
- button input
- lower-screen board touch behavior
- tap-to-restart from the WIN/LOSE popup
- drawing the upper-screen HUD counters and difficulty text
- sending tiny sound commands to ARM7 through IPCSYNC
- reading shared-memory touch state written by ARM7

## ARM7 Responsibilities

The custom ARM7 core handles:

- direct sound playback using DS sound hardware registers
- reading touch coordinates
- writing touch state into shared IPC RAM

It intentionally avoids the standard BlocksDS/libnds sound helper path because that path froze on DraStic.

## Sound Command Path

ARM9 sends a sound command through IPCSYNC:

```cpp
sendArm7SoundCommand(1); // move
sendArm7SoundCommand(2); // flag
sendArm7SoundCommand(3); // reveal
```

The command includes a toggle bit so repeated identical sounds are still detected by ARM7.

ARM7 polls IPCSYNC and plays the corresponding PCM sample.

## PCM Sample Source

The PCM arrays are stored in `source/main.cpp`:

```cpp
sfxMoveClick
sfxFlagClick
sfxRevealClick
```

The Makefile extracts those arrays and generates:

```text
build/custom_arm7/arm7_sound.c
```

This keeps a single source of truth for the exact sounds.

## Shared Touch Path

The custom ARM7 writes touch data into shared IPC RAM:

```text
0x027FF100 magic
0x027FF104 held
0x027FF106 x
0x027FF108 y
```

ARM9 reads that data with:

```cpp
readSharedArm7Touch(...)
```

This avoids relying on the normal ARM7-to-ARM9 system FIFO touch path, which was unreliable in DraStic with the custom ARM7 core.

## Upper Screen Artwork

The upper-screen background is stored as an already-scaled DS image:

```text
assets/top_screen_256x192.png
```

The Makefile converts it into:

```text
source/generated_top_screen_bg.h
```

At runtime, `drawTopScreenBackground()` copies the generated bitmap into the top-screen back buffer.

The dynamic mine counter, flag counter, and difficulty text are drawn by code over the background. This keeps the background clean while still allowing the HUD values to update.

## Rendering

Rendering uses software frame buffers:

```cpp
backBufferMain
backBufferSub
```

Then `presentFrame()` copies them to VRAM using CPU `memcpy`.

DMA copy was avoided because DraStic may internally use DMA channel 3 for audio, and `dmaCopyWords(3, ...)` could freeze if that channel is busy.
