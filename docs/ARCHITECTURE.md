# Architecture

## Overview

Game logic and rendering run on ARM9 in `source/main.cpp`.

The Makefile generates a custom ARM7 core at build time:

```text
build/custom_arm7/arm7_sound.c
build/custom_arm7/arm7_sound.elf
```

## ARM9

ARM9 handles game state, rendering, button input, lower-screen board touch logic, and sending sound commands.

## ARM7

ARM7 handles direct sound playback and touch sampling.

Sound commands are sent from ARM9 to ARM7 through IPCSYNC:

- 1: move click
- 2: flag click
- 3: reveal click

Touch state is written by ARM7 into shared IPC RAM:

```text
0x027FF100 magic
0x027FF104 held
0x027FF106 x
0x027FF108 y
```

ARM9 reads this through `readSharedArm7Touch()`.

## Why Custom ARM7

Standard BlocksDS/libnds audio helpers caused DraStic to freeze when sound was triggered. The custom ARM7 avoids that path and plays the PCM samples directly.

