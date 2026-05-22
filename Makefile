NAME        := ds-minesweeper
GAME_TITLE  := DS Minesweeper
GAME_SUBTITLE := B/I/E + drag scroll
GAME_AUTHOR := Omer

BLOCKSDS    ?= /opt/wonderful/thirdparty/blocksds/core
WONDERFUL_TOOLCHAIN ?= /opt/wonderful
ARM_NONE_EABI_PATH ?= $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/

ARM7_BUILDDIR := build/custom_arm7
ARM7_SRC := $(ARM7_BUILDDIR)/arm7_sound.c
ARM7ELF := $(ARM7_BUILDDIR)/arm7_sound.elf

# Generate a 256x192 RGB555 upper-screen background from the already-scaled PNG.
# The generated header is intentionally not committed.
define GENERATE_TOP_SCREEN_BG_PY
import pathlib
try:
    from PIL import Image
except ImportError:
    raise SystemExit("Pillow is required to build the top screen background. Install it with: apt-get install -y python3-pil")

root = pathlib.Path(__file__).resolve().parent
src = root / "assets" / "top_screen_256x192.png"
out = root / "source" / "generated_top_screen_bg.h"
if not src.exists():
    raise SystemExit("Missing assets/top_screen_256x192.png")

img = Image.open(src).convert("RGB")
if img.size != (256, 192):
    raise SystemExit(f"assets/top_screen_256x192.png must be exactly 256x192, got {img.size}")
values = []
for r, g, b in img.getdata():
    r5 = r >> 3
    g5 = g >> 3
    b5 = b >> 3
    values.append(0x8000 | r5 | (g5 << 5) | (b5 << 10))

with out.open("w") as f:
    f.write("#pragma once\n")
    f.write("static const u16 topScreenBg[256 * 192] = {\n")
    for i in range(0, len(values), 12):
        f.write("    " + ", ".join(f"0x{v:04X}" for v in values[i:i+12]) + ",\n")
    f.write("};\n")
endef

$(file >.generate_top_screen_bg.py,$(GENERATE_TOP_SCREEN_BG_PY))
$(shell python3 .generate_top_screen_bg.py)

.DEFAULT_GOAL := all

# Generate a custom ARM7 core from the exact PCM arrays in source/main.cpp.
# Sound avoids libnds/Maxmod audio FIFO entirely. Touch is written to shared IPC
# RAM so ARM9 does not depend on DraStic's system FIFO touch delivery.
define GENERATE_ARM7_SOUND_C_PY
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent
source = (root / "source" / "main.cpp").read_text()
outdir = root / "build" / "custom_arm7"
outdir.mkdir(parents=True, exist_ok=True)

samples = [
    ("sfxMoveClick", "move"),
    ("sfxFlagClick", "flag"),
    ("sfxRevealClick", "reveal"),
]

def extract(symbol):
    pattern = r"static\s+s16\s+" + symbol + r"\[\].*?=\s*\{(.*?)\};"
    match = re.search(pattern, source, re.S)
    if not match:
        raise SystemExit(f"Could not find {symbol} in source/main.cpp")
    return [int(x) for x in re.findall(r"-?\d+", match.group(1))]

lines = []
lines.append('#include <stdint.h>')
lines.append('#include <nds.h>')
lines.append('#include <nds/arm7/touch.h>')
lines.append('#define MS_REG_IPCSYNC (*(vu16*)0x04000180)')
lines.append('#define MS_REG_KEYXY (*(vu16*)0x04000136)')
lines.append('#define MS_REG_SOUNDCNT (*(vu32*)0x04000500)')
lines.append('#define MS_TOUCH_MAGIC (*(volatile uint32_t*)0x027FF100)')
lines.append('#define MS_TOUCH_HELD  (*(volatile uint16_t*)0x027FF104)')
lines.append('#define MS_TOUCH_X     (*(volatile uint16_t*)0x027FF106)')
lines.append('#define MS_TOUCH_Y     (*(volatile uint16_t*)0x027FF108)')
lines.append('#define MS_SCHANNEL_CR(ch) (*(vu32*)(0x04000400 + ((ch) * 0x10) + 0x00))')
lines.append('#define MS_SCHANNEL_SOURCE(ch) (*(vu32*)(0x04000400 + ((ch) * 0x10) + 0x04))')
lines.append('#define MS_SCHANNEL_TIMER(ch) (*(vu16*)(0x04000400 + ((ch) * 0x10) + 0x08))')
lines.append('#define MS_SCHANNEL_REPEAT(ch) (*(vu16*)(0x04000400 + ((ch) * 0x10) + 0x0A))')
lines.append('#define MS_SCHANNEL_LENGTH(ch) (*(vu32*)(0x04000400 + ((ch) * 0x10) + 0x0C))')
lines.append('#define SOUND_RATE 32768')
lines.append('#define SOUND_TIMER ((uint16_t)(0x10000 - (16777216 / SOUND_RATE)))')
lines.append('#define SOUND_CTRL(volume) ((1u << 31) | (1u << 29) | (2u << 27) | (((uint32_t)64) << 16) | ((volume) & 127))')
max_values = 0
for symbol, name in samples:
    values = extract(symbol)
    max_values = max(max_values, len(values))
    lines.append(f'static const int16_t {name}_sample[] __attribute__((aligned(4))) = {{')
    for i in range(0, len(values), 12):
        lines.append('    ' + ', '.join(str(v) for v in values[i:i+12]) + ',')
    lines.append('};')
    lines.append(f'static const uint32_t {name}_count = sizeof({name}_sample) / sizeof({name}_sample[0]);')
lines.append('#define play_buffer ((int16_t*)0x02000000)')
lines.append('static void play_sample(const int16_t *data, uint32_t count, uint8_t volume) {')
lines.append('    MS_REG_SOUNDCNT = 0x0000807F;')
lines.append('    MS_SCHANNEL_CR(8) = 0;')
lines.append('    for (uint32_t i = 0; i < count; i++) play_buffer[i] = data[i];')
lines.append('    for (volatile int i = 0; i < 16; i++) { }')
lines.append('    MS_SCHANNEL_SOURCE(8) = 0x02000000;')
lines.append('    MS_SCHANNEL_TIMER(8) = SOUND_TIMER;')
lines.append('    MS_SCHANNEL_REPEAT(8) = 0;')
lines.append('    MS_SCHANNEL_LENGTH(8) = (count * sizeof(int16_t)) >> 2;')
lines.append('    MS_SCHANNEL_CR(8) = SOUND_CTRL(volume);')
lines.append('}')
lines.append('int main(void) {')
lines.append('    irqInit();')
lines.append('    touchInit();')
lines.append('    MS_TOUCH_MAGIC = 0x54434831;')
lines.append('    MS_TOUCH_HELD = 0;')
lines.append('    MS_TOUCH_X = 0;')
lines.append('    MS_TOUCH_Y = 0;')
lines.append('    MS_REG_SOUNDCNT = 0x0000807F;')
lines.append('    touchPosition touch;')
lines.append('    uint16_t last = 0xFFFF;')
lines.append('    while (1) {')
lines.append('        touchReadXY(&touch);')
lines.append('        MS_TOUCH_HELD = ((MS_REG_KEYXY & (1 << 6)) == 0) ? 1 : 0;')
lines.append('        MS_TOUCH_X = touch.px;')
lines.append('        MS_TOUCH_Y = touch.py;')
lines.append('        uint16_t raw = MS_REG_IPCSYNC;')
lines.append('        uint16_t v = raw & 0x000F;')
lines.append('        if ((v & 7) == 0) v = (raw >> 8) & 0x000F;')
lines.append('        if (v != last) {')
lines.append('            last = v;')
lines.append('            switch (v & 7) {')
lines.append('                case 1: play_sample(move_sample, move_count, 124); break;')
lines.append('                case 2: play_sample(flag_sample, flag_count, 124); break;')
lines.append('                case 3: play_sample(reveal_sample, reveal_count, 124); break;')
lines.append('                default: break;')
lines.append('            }')
lines.append('        }')
lines.append('        for (volatile int i = 0; i < 8; i++) { }')
lines.append('    }')
lines.append('}')
(outdir / "arm7_sound.c").write_text('\n'.join(lines) + '\n')
endef

$(file >.generate_arm7_sound.py,$(GENERATE_ARM7_SOUND_C_PY))
$(shell python3 .generate_arm7_sound.py)

# Build the custom ARM7 ELF before BlocksDS' default ROM rules call ndstool.
# The default makefile reads ARM7ELF but doesn't know about this generated target.
$(shell mkdir -p $(ARM7_BUILDDIR) && $(ARM_NONE_EABI_PATH)arm-none-eabi-gcc -mcpu=arm7tdmi -mthumb -mthumb-interwork -O3 -ffunction-sections -fdata-sections -D__NDS__ -D__BLOCKSDS__ -DARM7 -I$(BLOCKSDS)/libs/libnds/include -L$(BLOCKSDS)/libs/libnds/lib -L$(BLOCKSDS)/libs/libc7/lib -Wl,-Map,$(ARM7_BUILDDIR)/arm7_sound.map -Wl,--gc-sections -nostdlib -T$(BLOCKSDS)/sys/crts/ds_arm7.ld -Wl,--no-warn-rwx-segments -o $(ARM7ELF) $(ARM7_SRC) $(BLOCKSDS)/sys/crts/ds_arm7_crt0.o -Wl,--start-group -lnds7 -lc -lgcc -Wl,--end-group)

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile