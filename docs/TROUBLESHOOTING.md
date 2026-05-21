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

The Makefile uses Python to generate the custom ARM7 source.

If `python3` is missing:

```bash
apt-get update
apt-get install -y python3
```

## Missing ARM7 ELF

The Makefile builds:

```text
build/custom_arm7/arm7_sound.elf
```

If `ndstool` says it cannot open that file, check that `ARM_NONE_EABI_PATH` ends with a slash:

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

## Bad Tone or Repeating Noise

This usually means the sound channel control bits are wrong.

The working generated control word is:

```c
#define SOUND_CTRL(volume) ((1u << 31) | (1u << 29) | (2u << 27) | (((uint32_t)64) << 16) | ((volume) & 127))
```

