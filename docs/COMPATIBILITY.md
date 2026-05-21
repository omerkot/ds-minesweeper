# Compatibility Notes

## Tested Targets

- melonDS
- DraStic

## Original Problem

The game worked in melonDS but froze in DraStic when the first sound was triggered.

The freeze happened when using standard BlocksDS/libnds sound helpers such as:

- soundPlaySample()
- soundPlaySampleChannel()
- soundPlayPSG()
- soundPlayPSGChannel()

Maxmod was also tested. It froze during Maxmod initialization on DraStic.

## Final Sound Solution

The final solution uses a custom generated ARM7 core.

ARM9 sends a tiny command through IPCSYNC. ARM7 plays the sample directly through sound hardware registers.

This avoids the standard libnds/Maxmod sound FIFO path that caused DraStic freezes.

## Touch Issue and Solution

Replacing the default ARM7 core broke touch input because the default ARM7 normally services touch input for ARM9.

The fix is custom shared-memory touch transfer:

- ARM7 reads touch state.
- ARM7 writes held/x/y into IPC RAM.
- ARM9 reads that shared state and applies it to the existing lower-screen touch logic.

## DraStic Note

DraStic sound must be enabled in emulator settings. If DraStic sound is muted, the game can run correctly but no sound will be heard.

## Top Screen Touch

The Nintendo DS only has touch on the lower screen.

The top-screen smiley and difficulty buttons are visual UI only. Use buttons instead:

- START: new game
- SELECT: cycle difficulty
- X/Y: change difficulty

