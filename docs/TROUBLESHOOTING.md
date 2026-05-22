# Troubleshooting

## Build

```bash
make clean
make
```

Output ROM:

```text
ds-minesweeper.nds
```

## Python Missing

The Makefile uses Python to generate build-time files.

If `python3` is missing:

```bash
apt-get update
apt-get install -y python3
```

## Pillow Missing

The top-screen background generator uses Pillow to read `assets/top_screen_256x192.png`.

If Pillow is missing:

```bash
apt-get update
apt-get install -y python3-pil
```

## Missing Upper-Screen Background

The build expects this file:

```text
assets/top_screen_256x192.png
```

It must be exactly 256x192 pixels. The Makefile converts it into:

```text
source/generated_top_screen_bg.h
```

## Missing ARM7 ELF

The Makefile builds:

```text
build/custom_arm7/arm7_sound.elf
```

before the final ROM is packaged.

If `ndstool` says it cannot open that file, check that the generated ARM7 compile command ran and that `ARM_NONE_EABI_PATH` ends with a slash:

```make
ARM_NONE_EABI_PATH ?= $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/
```

## DraStic Freezes on Sound

Do not switch back to standard `soundPlaySample()` or Maxmod unless intentionally testing.

The working path is the generated custom ARM7 core.

## No Sound on DraStic

Check DraStic audio settings first. Sound must be enabled in DraStic.

Then rebuild:

```bash
make clean
make
```

## Touch Problems

The custom ARM7 writes touch state into shared IPC RAM.

Check that:

- `Makefile` generates `MS_TOUCH_MAGIC`, `MS_TOUCH_HELD`, `MS_TOUCH_X`, and `MS_TOUCH_Y`
- `main.cpp` calls `readSharedArm7Touch(...)`
- the generated ARM7 core calls `touchInit()`
- the generated ARM7 core calls `touchReadXY(&touch)`

## Generated Files Showing Up in Git

Generated files should stay out of commits. The `.gitignore` should ignore:

```text
build/
.generate_*.py
source/generated_*.h
*.nds
```

If they appear in `git status`, remove them and rebuild when needed:

```bash
rm -rf build
rm -f .generate_*.py source/generated_*.h *.nds
make clean
make
```

## Bad Tone or Repeating Noise

This usually means the sound channel control bits are wrong.

The working generated control word is:

```c
#define SOUND_CTRL(volume) ((1u << 31) | (1u << 29) | (2u << 27) | (((uint32_t)64) << 16) | ((volume) & 127))
```
