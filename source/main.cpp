#include <nds.h>
#include <stdlib.h>

static const int MAX_W = 30;
static const int MAX_H = 16;
static const int TILE = 16;
static const int BOARD_FRAME = 8;  // thick frame outside playable cells, never covering cells
static const int VIEW_X = 0;
static const int VIEW_Y = 0;
static const int VIEW_W = 256;
static const int VIEW_H = 192;
static const int LONG_TAP_FRAMES = 30;
static const int DRAG_THRESHOLD = 6;
static const int KEY_REPEAT_DELAY = 7;   // about 1/8 second before held d-pad repeats
static const int KEY_REPEAT_RATE = 4;    // controlled repeat speed for DraStic handhelds
static const int PAN_SPEED = 10;         // pixels per frame for L/R panning

static int cursorStepForHold(int frames) {
    // Balanced: immediate one-tile nudges, then only mild acceleration.
    if (frames > 55) return 2;
    return 1;
}

struct Difficulty {
    int w;
    int h;
    int mines;
};

static const Difficulty difficulties[3] = {
    {9, 9, 10},    // Beginner
    {16, 16, 40},  // Intermediate
    {30, 16, 99}   // Expert
};

struct Cell {
    bool mine;
    bool revealed;
    bool flagged;
    int neighborMines;
};

static Cell board[MAX_H][MAX_W];
static int difficultyIndex = 0;
static int boardW = 9;
static int boardH = 9;
static int mineCount = 10;
static bool gameOver = false;
static bool won = false;
static bool firstTap = true;
static int flagsPlaced = 0;
static int camX = 0;
static int camY = 0;
static int cursorX = 0;
static int cursorY = 0;

static bool touchTracking = false;
static bool isDragging = false;
static bool longTapTriggered = false;
static int touchStartPx = 0;
static int touchStartPy = 0;
static int lastTouchPx = 0;
static int lastTouchPy = 0;
static int touchHeldFrames = 0;
static int touchStartTileX = -1;
static int touchStartTileY = -1;

static u16 *fbCurrent = nullptr;
static u16 *vramMain = nullptr;
static u16 *vramSub = nullptr;
static u16 backBufferMain[256 * 192];
static u16 backBufferSub[256 * 192];

static inline u16 C(int r, int g, int b) { return RGB15(r, g, b) | BIT(15); }

static const u16 COL_BG       = C(16, 16, 16);
static const u16 COL_PANEL    = C(23, 23, 23);
static const u16 COL_TILE     = C(21, 21, 21);
static const u16 COL_TILE_HI  = C(31, 31, 31);
static const u16 COL_TILE_SH  = C(9, 9, 9);
static const u16 COL_OPEN     = C(18, 18, 18);
static const u16 COL_OPEN_DK  = C(10, 10, 10);
static const u16 COL_GRID     = C(5, 5, 5);
static const u16 COL_BLACK    = C(0, 0, 0);
static const u16 COL_WHITE    = C(31, 31, 31);
static const u16 COL_RED      = C(31, 2, 2);
static const u16 COL_DARK_RED = C(18, 0, 0);
static const u16 COL_YELLOW   = C(31, 27, 0);
static const u16 COL_FACE_DK  = C(18, 14, 0);
static const u16 COL_BLUE     = C(4, 8, 31);
static const u16 COL_GREEN    = C(0, 20, 0);
static const u16 COL_BUTTON   = C(20, 20, 20);
static const u16 COL_ACTIVE   = C(31, 25, 5);
static const u16 COL_BORDER   = C(31, 31, 31);
static const u16 COL_BORDER_SH= C(0, 0, 0);
static const u16 COL_EDGE_FILL= C(24, 20, 4);

static const u16 numberColors[9] = {
    C(0, 0, 0), C(0, 6, 31), C(0, 18, 0), C(28, 0, 0), C(0, 0, 16),
    C(18, 0, 0), C(0, 18, 18), C(0, 0, 0), C(12, 12, 12)
};

static const int SFX_RATE = 32768;
static int moveSfxCooldownFrames = 0;
static u32 frameCounter = 0;
static u32 rngState = 0xA341316Cu;

static void drawGame();
static void presentFrame();

static volatile u16 &ipcSyncReg() {
    return *(volatile u16*)0x04000180;
}

static bool readSharedArm7Touch(touchPosition &touch, bool &touchHeld) {
    volatile u32 *magic = (volatile u32*)0x027FF100;
    if (*magic != 0x54434831) return false;

    volatile u16 *held = (volatile u16*)0x027FF104;
    volatile u16 *x = (volatile u16*)0x027FF106;
    volatile u16 *y = (volatile u16*)0x027FF108;

    touchHeld = (*held != 0);
    touch.px = *x;
    touch.py = *y;
    return true;
}

// Short 16-bit PCM transients — intentional click/noise envelopes.
static s16 sfxMoveClick[] __attribute__((aligned(4), unused)) = {
    -1151, -15777, 9232, -17660, 5396, -8248, -8853, -3858, -9114, -5771, -7896, -14749,
    1724, 4794, -5952, -10371, 6535, 7320, 5105, -5605, 13879, -13098, 10784, -7433,
    -4449, -10617, -1003, 3407, -3395, -1108, 5107, -4795, 3296, -9876, -5000, -7105,
    5132, -3330, -684, -826, 1371, -4850, 6043, 785, -1519, -860, 2093, 2875,
    4437, -4123, 7071, -5801, 639, 1222, -2122, -1518, -3106, 257, 3729, -608,
    4490, -2766, 2762, -372, 1711, -1390, 3510, 2150, 791, 154, -1925, 378,
    1745, 2064, 2626, -1956, 142, 142, -1667, -858, -882, -2337, -1290, 527,
    -918, -1514, 118, 821, -954, -632, 603, 736, 1341, 623, -216, -579,
    0, 570, 1342, -1048, -373, -796, -233, -275, 386, -602, -536, -318,
    14, -96, 721, 57, 169, -24, 303, -527, 455, 109, 380, 108,
    21, -144, -141, -3, -140, -250, -60, -176, -6, -181, -100, -124,
    -59, -57, -61, 38, 37, -63, -12, -29, 0, -38, 31, 19,
    4, -5, -6, -9, 0, -3, 2, -2, 0, 0, 0, 0,
    0, 0, 0, 0
};
static s16 sfxFlagClick[] __attribute__((aligned(4), unused)) = {
    8393, 7092, -345, 3968, 1043, 14797, 9809, 14498, 4309, 968, 11488, 13484,
    9486, 7434, 6829, 4763, -5446, 191, -2826, -9004, -8979, -4247, -5045, 2728,
    6967, -4281, 4114, 3815, 1951, -11101, -14748, -14787, -15017, -13841, -3046, 5322,
    6626, 1123, 7838, 13057, -1267, 11834, 16795, 13580, 11884, 5739, -587, 8150,
    -244, 5761, 7228, -1209, -1302, 5292, 2529, -3570, -3781, -3374, 3702, 2385,
    -3642, 1502, 2085, -811, -3265, -2163, -4771, -5242, 296, -1133, -1325, 1058,
    -713, 1656, 1867, -185, 299, 664, 581, 1751, 561, 912, -169, 1920,
    90, 200, 383, 1078, -188, 952, -12, 68, 62, -932, -114, -615,
    -963, 308, -795, -407, -122, -428, -765, -514, -439, -122, -773, -187,
    -376, -226, 310, 168, 272, 450, 558, 238, 318, 220, 182, 226,
    71, 73, 24, 173, 70, 114, 123, -63, 15, 98, 68, -67,
    -68, -21, -75, -55, -73, -16, -9, -2, -47, -10, -9, -24,
    5, 8, -4, 7, 1, 1, 3, 2, 0, 0, 0, 0,
    0, 0, 0, 0
};
static s16 sfxRevealClick[] __attribute__((aligned(4), unused)) = {
    6151, 7968, 2193, 7417, 9729, 3195, -9571, 1402, -4690, -2094, -13625, -6838,
    -5248, 10220, 5192, 3149, 4750, 7187, 1160, -9869, -12474, -1181, -2052, 3994,
    433, 1910, 8342, 2158, -5925, -117, -4987, -2233, -6316, 5376, 742, 9148,
    3563, -925, -2583, -1405, -6573, -5016, 1747, 2095, 1577, 104, -3858, -5474,
    -5508, -4041, 1903, 1540, 3807, 528, -1142, -5465, -4802, -4638, 1823, 2937,
    1390, 1173, 935, -4484, -910, -712, 1667, 3862, 1058, -519, -1362, -227,
    -955, 2497, 2670, 1458, -980, -2203, -2613, -882, 265, 2041, -374, -1881,
    -2325, 312, 1598, 1601, 104, -1106, -1493, -609, -74, 897, 118, 236,
    -196, -328, 104, 1326, 50, -604, -1169, -264, 431, 514, -250, -449,
    -760, -62, 134, 156, -494, -633, -181, 121, 290, -43, -150, -61,
    222, 329, -32, -220, 26, -7, 189, 41, -198, -10, 118, 104,
    30, -33, -71, 43, 45, 13, -20, 11, 31, 33, -7, -11,
    -2, 2, -4, -7, -4, 4, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0
};


// DraStic compatibility note:
// All standard BlocksDS/libnds/Maxmod sound paths froze on the first ARM7/FIFO
// sound command. This custom path avoids FIFO entirely: ARM9 writes a tiny
// command into IPCSYNC, and the custom ARM7 core polls it and plays the exact
// PCM arrays directly on the ARM7 sound hardware.
static void sendArm7SoundCommand(u8 command) {
    static u8 toggle = 0;
    toggle ^= 8;
    u16 encoded = (u16)((command & 7) | toggle);

    ipcSyncReg() = (u16)(encoded << 8);
}

// ── VBlank sync ───────────────────────────────────────────────────────────────
// Use libnds' normal VBlank wait. The previous manual REG_DISPSTAT polling can
// spin forever on emulator/display timing differences, which looks exactly like
// a frozen first frame with no input response.
static void waitFrame() {
    swiWaitForVBlank();
    frameCounter++;
    if (moveSfxCooldownFrames > 0) moveSfxCooldownFrames--;
}




static void playMoveSound() {
    if (moveSfxCooldownFrames > 0) return;
    moveSfxCooldownFrames = 2;
    sendArm7SoundCommand(1);
}

static void playFlagSound()   { sendArm7SoundCommand(2); }
static void playRevealSound() { sendArm7SoundCommand(3); }


static u32 mixEntropy(u32 v) {
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    v ^= v >> 16;
    return v ? v : 0xA341316Cu;
}

static void reseedRng(int safeX, int safeY) {
    u32 seed = 0x9E3779B9u;
    seed ^= frameCounter * 0x85ebca6bu;
    seed ^= ((u32)(safeX + 17) << 24) ^ ((u32)(safeY + 31) << 16);
    seed ^= ((u32)(difficultyIndex + 1) * 0x27d4eb2du);
    seed ^= ((u32)REG_VCOUNT << 8);
    seed ^= (u32)keysHeld() * 0x165667b1u;
    rngState ^= mixEntropy(seed);
    rngState = mixEntropy(rngState);
}

static u32 nextRandom() {
    // xorshift32: small, fast, and better for repeated board generation than
    // using a fixed srand(12345) seed.
    u32 x = rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState = x ? x : 0xA341316Cu;
    return rngState;
}

static int randomInt(int limit) {
    return limit > 0 ? (int)(nextRandom() % (u32)limit) : 0;
}

static inline bool inBounds(int x, int y) {
    return x >= 0 && x < boardW && y >= 0 && y < boardH;
}

static void putPixelTo(u16 *fb, int x, int y, u16 color) {
    if (!fb || x < 0 || x >= 256 || y < 0 || y >= 192) return;
    fb[y * 256 + x] = color;
}

static void putPixel(int x, int y, u16 color) {
    putPixelTo(fbCurrent, x, y, color);
}

static void fillRect(int x, int y, int w, int h, u16 color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > 256 ? 256 : x + w;
    int y1 = y + h > 192 ? 192 : y + h;
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) putPixel(xx, yy, color);
    }
}

static void drawRect(int x, int y, int w, int h, u16 color) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; i++) {
        putPixel(x + i, y, color);
        putPixel(x + i, y + h - 1, color);
    }
    for (int i = 0; i < h; i++) {
        putPixel(x, y + i, color);
        putPixel(x + w - 1, y + i, color);
    }
}


// Board numbers intentionally keep the older chunkier 3x5 Minesweeper-style font.
// The upper screen uses the cleaner DS menu 5x7 font.
static const unsigned char boardDigitFont[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7}
};

static void drawBoardDigit(int x, int y, int digit, int scale, u16 color) {
    if (digit < 0 || digit > 9) return;
    for (int row = 0; row < 5; row++) {
        unsigned char bits = boardDigitFont[digit][row];
        for (int col = 0; col < 3; col++) {
            if (bits & (1 << (2 - col))) {
                fillRect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}



// Clean DS-menu-style 5x7 bitmap font. It is still tiny and fast, but much more readable
// than the earlier chunky 3x5 debug font.
static const unsigned char glyph5x7(char ch, int row) {
    const unsigned char BLANK[7] = {0,0,0,0,0,0,0};
    const unsigned char *g = BLANK;
    static const unsigned char A[7]={14,17,17,31,17,17,17};
    static const unsigned char B[7]={30,17,17,30,17,17,30};
    static const unsigned char C[7]={14,17,16,16,16,17,14};
    static const unsigned char D[7]={30,17,17,17,17,17,30};
    static const unsigned char E[7]={31,16,16,30,16,16,31};
    static const unsigned char F[7]={31,16,16,30,16,16,16};
    static const unsigned char G[7]={14,17,16,23,17,17,14};
    static const unsigned char H[7]={17,17,17,31,17,17,17};
    static const unsigned char I[7]={31,4,4,4,4,4,31};
    static const unsigned char J[7]={1,1,1,1,17,17,14};
    static const unsigned char K[7]={17,18,20,24,20,18,17};
    static const unsigned char L[7]={16,16,16,16,16,16,31};
    static const unsigned char M[7]={17,27,21,21,17,17,17};
    static const unsigned char N[7]={17,25,21,19,17,17,17};
    static const unsigned char O[7]={14,17,17,17,17,17,14};
    static const unsigned char P[7]={30,17,17,30,16,16,16};
    static const unsigned char Q[7]={14,17,17,17,21,18,13};
    static const unsigned char R[7]={30,17,17,30,20,18,17};
    static const unsigned char S[7]={15,16,16,14,1,1,30};
    static const unsigned char T[7]={31,4,4,4,4,4,4};
    static const unsigned char U[7]={17,17,17,17,17,17,14};
    static const unsigned char V[7]={17,17,17,17,17,10,4};
    static const unsigned char W[7]={17,17,17,21,21,21,10};
    static const unsigned char X[7]={17,17,10,4,10,17,17};
    static const unsigned char Y[7]={17,17,10,4,4,4,4};
    static const unsigned char Z[7]={31,1,2,4,8,16,31};
    static const unsigned char N0[7]={14,17,19,21,25,17,14};
    static const unsigned char N1[7]={4,12,4,4,4,4,14};
    static const unsigned char N2[7]={14,17,1,2,4,8,31};
    static const unsigned char N3[7]={30,1,1,14,1,1,30};
    static const unsigned char N4[7]={2,6,10,18,31,2,2};
    static const unsigned char N5[7]={31,16,16,30,1,1,30};
    static const unsigned char N6[7]={14,16,16,30,17,17,14};
    static const unsigned char N7[7]={31,1,2,4,8,8,8};
    static const unsigned char N8[7]={14,17,17,14,17,17,14};
    static const unsigned char N9[7]={14,17,17,15,1,1,14};
    static const unsigned char COLON[7]={0,4,4,0,4,4,0};
    static const unsigned char DASH[7]={0,0,0,31,0,0,0};

    switch (ch) {
        case 'A': g=A; break; case 'B': g=B; break; case 'C': g=C; break; case 'D': g=D; break;
        case 'E': g=E; break; case 'F': g=F; break; case 'G': g=G; break; case 'H': g=H; break;
        case 'I': g=I; break; case 'J': g=J; break; case 'K': g=K; break; case 'L': g=L; break;
        case 'M': g=M; break; case 'N': g=N; break; case 'O': g=O; break; case 'P': g=P; break;
        case 'Q': g=Q; break; case 'R': g=R; break; case 'S': g=S; break; case 'T': g=T; break;
        case 'U': g=U; break; case 'V': g=V; break; case 'W': g=W; break; case 'X': g=X; break;
        case 'Y': g=Y; break; case 'Z': g=Z; break;
        case '0': g=N0; break; case '1': g=N1; break; case '2': g=N2; break; case '3': g=N3; break;
        case '4': g=N4; break; case '5': g=N5; break; case '6': g=N6; break; case '7': g=N7; break;
        case '8': g=N8; break; case '9': g=N9; break; case ':': g=COLON; break; case '-': g=DASH; break;
        default: g=BLANK; break;
    }
    return g[row];
}

static void drawGlyph5x7(int x, int y, char ch, int scale, u16 color) {
    for (int row = 0; row < 7; row++) {
        unsigned char bits = glyph5x7(ch, row);
        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col))) {
                fillRect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void drawDigit(int x, int y, int digit, int scale, u16 color) {
    if (digit < 0 || digit > 9) return;
    drawGlyph5x7(x, y, char('0' + digit), scale, color);
}

static void drawNumberFixed2(int x, int y, int value, int scale, u16 color) {
    if (value < 0) value = 0;
    if (value > 99) value = 99;
    int digitW = 5 * scale;
    int gap = scale;
    drawDigit(x, y, (value / 10) % 10, scale, color);
    drawDigit(x + digitW + gap, y, value % 10, scale, color);
}

static void drawLetter(int x, int y, char ch, int scale, u16 color) {
    drawGlyph5x7(x, y, ch, scale, color);
}

static void drawText(const char *text, int x, int y, int scale, u16 color) {
    int cursor = x;
    for (int i = 0; text[i]; i++) {
        if (text[i] == ' ') cursor += 4 * scale;
        else { drawGlyph5x7(cursor, y, text[i], scale, color); cursor += 6 * scale; }
    }
}

static int textWidth(const char *text, int scale) {
    int w = 0;
    for (int i = 0; text[i]; i++) w += (text[i] == ' ') ? 4 * scale : 6 * scale;
    return w;
}

static void drawCenteredText(const char *text, int centerX, int y, int scale, u16 color) {
    drawText(text, centerX - textWidth(text, scale) / 2, y, scale, color);
}

static void drawMineShape(int cx, int cy, u16 color) {
    fillRect(cx - 4, cy - 4, 9, 9, color);
    fillRect(cx - 6, cy - 2, 13, 5, color);
    fillRect(cx - 2, cy - 6, 5, 13, color);
    putPixel(cx - 6, cy - 6, color); putPixel(cx + 6, cy - 6, color);
    putPixel(cx - 6, cy + 6, color); putPixel(cx + 6, cy + 6, color);
    putPixel(cx - 2, cy - 2, COL_WHITE);
}

static void drawFlagShape(int x, int y) {
    fillRect(x + 7, y + 3, 2, 10, COL_BLACK);
    fillRect(x + 4, y + 13, 9, 2, COL_BLACK);
    fillRect(x + 9, y + 3, 5, 3, COL_RED);
    fillRect(x + 9, y + 6, 3, 3, COL_DARK_RED);
}

static void drawButton(int x, int y, int w, int h, bool active, char label) {
    fillRect(x, y, w, h, active ? COL_ACTIVE : COL_BUTTON);
    fillRect(x, y, w, 2, COL_TILE_HI);
    fillRect(x, y, 2, h, COL_TILE_HI);
    fillRect(x, y + h - 2, w, 2, COL_TILE_SH);
    fillRect(x + w - 2, y, 2, h, COL_TILE_SH);
    drawRect(x, y, w, h, COL_GRID);
    drawLetter(x + (w - 10) / 2, y + (h - 14) / 2, label, 2, active ? COL_BLACK : COL_WHITE);
}


static void drawStatusFace(int x, int y, int w, int h) {
    fillRect(x, y, w, h, COL_YELLOW);
    fillRect(x, y, w, 2, COL_TILE_HI);
    fillRect(x, y, 2, h, COL_TILE_HI);
    fillRect(x, y + h - 2, w, 2, COL_FACE_DK);
    fillRect(x + w - 2, y, 2, h, COL_FACE_DK);
    drawRect(x, y, w, h, COL_BLACK);

    if (won) {
        // Happy winning face with sunglasses.
        fillRect(x + 6, y + 7, 6, 4, COL_BLACK);
        fillRect(x + w - 12, y + 7, 6, 4, COL_BLACK);
        fillRect(x + 12, y + 9, w - 24, 2, COL_BLACK);
        fillRect(x + 8, y + h - 9, w - 16, 2, COL_BLACK);
        putPixel(x + 7, y + h - 10, COL_BLACK);
        putPixel(x + w - 8, y + h - 10, COL_BLACK);
    } else if (gameOver) {
        // Sad losing face.
        fillRect(x + 7, y + 7, 3, 3, COL_BLACK);
        fillRect(x + w - 10, y + 7, 3, 3, COL_BLACK);
        fillRect(x + 8, y + h - 8, w - 16, 2, COL_RED);
        putPixel(x + 7, y + h - 7, COL_RED);
        putPixel(x + w - 8, y + h - 7, COL_RED);
    } else {
        // Normal smile.
        fillRect(x + 7, y + 7, 3, 3, COL_BLACK);
        fillRect(x + w - 10, y + 7, 3, 3, COL_BLACK);
        fillRect(x + 8, y + h - 10, w - 16, 2, COL_BLACK);
        putPixel(x + 7, y + h - 11, COL_BLACK);
        putPixel(x + w - 8, y + h - 11, COL_BLACK);
    }
}

static void clampCamera() {
    int maxX = boardW * TILE + BOARD_FRAME * 2 - VIEW_W;
    int maxY = boardH * TILE + BOARD_FRAME * 2 - VIEW_H;
    if (maxX < 0) maxX = 0;
    if (maxY < 0) maxY = 0;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > maxX) camX = maxX;
    if (camY > maxY) camY = maxY;
}

static void drawTile(int tx, int ty) {
    int boardPxW = boardW * TILE + BOARD_FRAME * 2;
    int boardPxH = boardH * TILE + BOARD_FRAME * 2;
    int boardLeft = VIEW_X + (boardPxW < VIEW_W ? (VIEW_W - boardPxW) / 2 : 0);
    int boardTop = VIEW_Y + (boardPxH < VIEW_H ? (VIEW_H - boardPxH) / 2 : 0);
    int x = boardLeft + BOARD_FRAME + tx * TILE - camX;
    int y = boardTop + BOARD_FRAME + ty * TILE - camY;
    if (x <= -TILE || x >= 256 || y <= VIEW_Y - TILE || y >= 192) return;

    const Cell &cell = board[ty][tx];
    if (!cell.revealed) {
        fillRect(x, y, TILE, TILE, COL_TILE);
        fillRect(x, y, TILE, 2, COL_TILE_HI);
        fillRect(x, y, 2, TILE, COL_TILE_HI);
        fillRect(x, y + TILE - 2, TILE, 2, COL_TILE_SH);
        fillRect(x + TILE - 2, y, 2, TILE, COL_TILE_SH);
        drawRect(x, y, TILE, TILE, COL_GRID);
        if (cell.flagged) drawFlagShape(x, y);
        return;
    }

    fillRect(x, y, TILE, TILE, COL_OPEN);
    drawRect(x, y, TILE, TILE, COL_OPEN_DK);
    if (cell.mine) {
        if (gameOver && !won) fillRect(x + 2, y + 2, TILE - 4, TILE - 4, COL_RED);
        drawMineShape(x + TILE / 2, y + TILE / 2, COL_BLACK);
    } else if (cell.neighborMines > 0) {
        drawBoardDigit(x + 5, y + 3, cell.neighborMines, 2, numberColors[cell.neighborMines]);
    }
}


static void ensureCursorVisible() {
    int boardPxW = boardW * TILE + BOARD_FRAME * 2;
    int boardPxH = boardH * TILE + BOARD_FRAME * 2;
    int boardLeft = VIEW_X + (boardPxW < VIEW_W ? (VIEW_W - boardPxW) / 2 : 0);
    int boardTop = VIEW_Y + (boardPxH < VIEW_H ? (VIEW_H - boardPxH) / 2 : 0);

    int screenX = boardLeft + BOARD_FRAME + cursorX * TILE - camX;
    int screenY = boardTop + BOARD_FRAME + cursorY * TILE - camY;

    // Keep the selected tile visible, but when the cursor is on an outer edge,
    // also keep the outside frame visible. This fixes the old bug where the
    // right/bottom frame stayed just off-screen and the left/top frame would not
    // come back when returning to the edge.
    int marginLeft = (cursorX == 0) ? BOARD_FRAME : 0;
    int marginRight = (cursorX == boardW - 1) ? BOARD_FRAME : 0;
    int marginTop = (cursorY == 0) ? BOARD_FRAME : 0;
    int marginBottom = (cursorY == boardH - 1) ? BOARD_FRAME : 0;

    if (screenX - marginLeft < VIEW_X) camX -= (VIEW_X - (screenX - marginLeft));
    if (screenX + TILE + marginRight > VIEW_X + VIEW_W) camX += (screenX + TILE + marginRight - (VIEW_X + VIEW_W));
    if (screenY - marginTop < VIEW_Y) camY -= (VIEW_Y - (screenY - marginTop));
    if (screenY + TILE + marginBottom > VIEW_Y + VIEW_H) camY += (screenY + TILE + marginBottom - (VIEW_Y + VIEW_H));

    clampCamera();
}

static void drawCursor() {
    int boardPxW = boardW * TILE + BOARD_FRAME * 2;
    int boardPxH = boardH * TILE + BOARD_FRAME * 2;
    int boardLeft = VIEW_X + (boardPxW < VIEW_W ? (VIEW_W - boardPxW) / 2 : 0);
    int boardTop = VIEW_Y + (boardPxH < VIEW_H ? (VIEW_H - boardPxH) / 2 : 0);
    int x = boardLeft + BOARD_FRAME + cursorX * TILE - camX;
    int y = boardTop + BOARD_FRAME + cursorY * TILE - camY;
    if (x <= -TILE || x >= 256 || y <= VIEW_Y - TILE || y >= 192) return;
    drawRect(x, y, TILE, TILE, COL_YELLOW);
    drawRect(x + 1, y + 1, TILE - 2, TILE - 2, COL_YELLOW);
}

static void drawScrollBars() {
    int boardPxW = boardW * TILE + BOARD_FRAME * 2;
    int boardPxH = boardH * TILE + BOARD_FRAME * 2;
    if (boardPxW > VIEW_W) {
        int barW = (VIEW_W * VIEW_W) / boardPxW;
        if (barW < 20) barW = 20;
        int maxX = boardPxW - VIEW_W;
        int x = (camX * (VIEW_W - barW)) / maxX;
        fillRect(x, 189, barW, 3, COL_WHITE);
    }
    if (boardPxH > VIEW_H) {
        int barH = (VIEW_H * VIEW_H) / boardPxH;
        if (barH < 20) barH = 20;
        int maxY = boardPxH - VIEW_H;
        int y = VIEW_Y + (camY * (VIEW_H - barH)) / maxY;
        fillRect(253, y, 3, barH, COL_WHITE);
    }
}


static void drawMiniMap() {
    // Small board-position indicator in the top panel: full board outline + current viewport.
    int x = 97, y = 158, w = 62, h = 26;
    fillRect(x, y, w, h, C(8, 8, 8));
    drawRect(x, y, w, h, COL_BORDER);

    int boardPxW = boardW * TILE + BOARD_FRAME * 2;
    int boardPxH = boardH * TILE + BOARD_FRAME * 2;
    int viewW = boardPxW <= VIEW_W ? w - 4 : (VIEW_W * (w - 4)) / boardPxW;
    int viewH = boardPxH <= VIEW_H ? h - 4 : (VIEW_H * (h - 4)) / boardPxH;
    if (viewW < 5) viewW = 5;
    if (viewH < 5) viewH = 5;

    int maxX = boardPxW - VIEW_W;
    int maxY = boardPxH - VIEW_H;
    if (maxX < 1) maxX = 1;
    if (maxY < 1) maxY = 1;
    int vx = x + 2 + (camX * ((w - 4) - viewW)) / maxX;
    int vy = y + 2 + (camY * ((h - 4) - viewH)) / maxY;
    fillRect(vx, vy, viewW, viewH, COL_YELLOW);
    drawRect(vx, vy, viewW, viewH, COL_BLACK);
}


static void drawBoardFrame() {
    // Draw four independent strips just outside the playable cell rectangle.
    // This is simpler and more reliable than drawing one big world rectangle:
    // the cells never get covered, and each side appears exactly when that
    // board edge is inside the current viewport.
    int worldW = boardW * TILE + BOARD_FRAME * 2;
    int worldH = boardH * TILE + BOARD_FRAME * 2;
    int boardLeft = VIEW_X + (worldW < VIEW_W ? (VIEW_W - worldW) / 2 : 0);
    int boardTop = VIEW_Y + (worldH < VIEW_H ? (VIEW_H - worldH) / 2 : 0);

    int cellX = boardLeft + BOARD_FRAME - camX;
    int cellY = boardTop + BOARD_FRAME - camY;
    int cellsW = boardW * TILE;
    int cellsH = boardH * TILE;

    // Gold body of the frame, outside cells only.
    fillRect(cellX - BOARD_FRAME, cellY - BOARD_FRAME, cellsW + BOARD_FRAME * 2, BOARD_FRAME, COL_EDGE_FILL); // top
    fillRect(cellX - BOARD_FRAME, cellY + cellsH,      cellsW + BOARD_FRAME * 2, BOARD_FRAME, COL_EDGE_FILL); // bottom
    fillRect(cellX - BOARD_FRAME, cellY,               BOARD_FRAME, cellsH, COL_EDGE_FILL);                  // left
    fillRect(cellX + cellsW,      cellY,               BOARD_FRAME, cellsH, COL_EDGE_FILL);                  // right

    // Dark outer lip.
    drawRect(cellX - BOARD_FRAME, cellY - BOARD_FRAME,
             cellsW + BOARD_FRAME * 2, cellsH + BOARD_FRAME * 2, COL_BORDER_SH);
    drawRect(cellX - BOARD_FRAME + 1, cellY - BOARD_FRAME + 1,
             cellsW + BOARD_FRAME * 2 - 2, cellsH + BOARD_FRAME * 2 - 2, COL_WHITE);

    // Thin dark separator around the playable grid, still outside the cells.
    drawRect(cellX - 1, cellY - 1, cellsW + 2, cellsH + 2, COL_BLACK);
}

static void drawTopScreen() {
    fillRect(0, 0, 256, 192, C(7, 10, 16));

    // Title panel
    fillRect(8, 8, 240, 38, C(12, 16, 24));
    drawRect(8, 8, 240, 38, COL_TILE_HI);
    drawCenteredText("MINESWEEPER DS", 129, 21, 2, COL_BLACK);
    drawCenteredText("MINESWEEPER DS", 128, 20, 2, COL_YELLOW);

    // Clean stats panel: mines left, face centered, flags right.
    fillRect(8, 54, 240, 58, COL_PANEL);
    drawRect(8, 54, 240, 58, COL_GRID);

    // Symmetric stat groups around the center face:
    // mines: icon then number; flags: number then icon.
    // The two numeric counters are mirrored around the smiley.
    drawMineShape(28, 82, COL_BLACK);
    // Counters are centered in the open space between the icon and the face.
    drawNumberFixed2(49, 72, mineCount - flagsPlaced, 3, COL_RED);

    drawStatusFace(112, 67, 32, 30);

    drawNumberFixed2(175, 72, flagsPlaced, 3, COL_YELLOW);
    drawFlagShape(219, 73);

    // Difficulty buttons.
    drawButton(30, 124, 42, 28, difficultyIndex == 0, 'B');
    drawButton(107, 124, 42, 28, difficultyIndex == 1, 'I');
    drawButton(184, 124, 42, 28, difficultyIndex == 2, 'E');

    // Small unobtrusive board-position map at the bottom of the top screen.
    drawMiniMap();

    // Centered board-position indicator replaces the old control hints.
}


static void drawEndOverlay() {
    if (!gameOver) return;

    int x = 24;
    int y = 48;
    int w = 208;
    int h = 96;
    fillRect(x + 4, y + 4, w, h, C(5, 5, 5));
    fillRect(x, y, w, h, won ? C(6, 18, 6) : C(18, 4, 4));
    drawRect(x, y, w, h, COL_WHITE);
    drawRect(x + 2, y + 2, w - 4, h - 4, COL_BLACK);

    if (won) {
        drawCenteredText("WIN", 130, y + 24, 5, COL_BLACK);
        drawCenteredText("WIN", 128, y + 22, 5, COL_YELLOW);
    } else {
        drawCenteredText("LOSE", 130, y + 24, 5, COL_BLACK);
        drawCenteredText("LOSE", 128, y + 22, 5, COL_RED);
    }

    drawCenteredText("START", 128, y + 72, 2, COL_WHITE);
}

static void drawBoardScreen() {
    fillRect(0, 0, 256, 192, COL_BG);
    drawBoardFrame();
    for (int y = 0; y < boardH; y++) for (int x = 0; x < boardW; x++) drawTile(x, y);
    drawCursor();
    drawRect(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, COL_GRID);
    drawScrollBars();
    drawEndOverlay();
}

static void drawGame() {
    fbCurrent = backBufferMain;
    drawTopScreen();
    fbCurrent = backBufferSub;
    drawBoardScreen();
}

static void presentFrame() {
    waitFrame();
    // DraStic commonly drives its audio output via DMA channel 3 internally.
    // dmaCopyWords(3, ...) spin-waits until the channel is free; if DraStic has
    // already claimed it the wait never ends and the screen freezes after startup.
    // CPU memcpy has no DMA dependency and works on every emulator.
    // VRAM on NDS is mapped uncached so no DC_FlushAll is needed before a CPU copy.
    memcpy(vramMain, backBufferMain, 256 * 192 * sizeof(u16));
    memcpy(vramSub,  backBufferSub,  256 * 192 * sizeof(u16));
}

static void resetCell(Cell &cell) {
    cell.mine = false; cell.revealed = false; cell.flagged = false; cell.neighborMines = 0;
}

static void clearBoard() {
    for (int y = 0; y < MAX_H; y++) for (int x = 0; x < MAX_W; x++) resetCell(board[y][x]);
}

static bool isSafeFirstArea(int x, int y, int safeX, int safeY) {
    return abs(x - safeX) <= 1 && abs(y - safeY) <= 1;
}

static void placeMines(int safeX, int safeY) {
    // Build a list of all allowed cells, then shuffle it. This avoids repeated
    // rand() collisions and gives a more even board distribution.
    int candidates[MAX_W * MAX_H];
    int count = 0;

    for (int y = 0; y < boardH; y++) {
        for (int x = 0; x < boardW; x++) {
            if (!isSafeFirstArea(x, y, safeX, safeY)) {
                candidates[count++] = y * boardW + x;
            }
        }
    }

    for (int i = count - 1; i > 0; i--) {
        int j = randomInt(i + 1);
        int tmp = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = tmp;
    }

    int toPlace = mineCount < count ? mineCount : count;
    for (int i = 0; i < toPlace; i++) {
        int idx = candidates[i];
        int x = idx % boardW;
        int y = idx / boardW;
        board[y][x].mine = true;
    }
}

static void calculateNumbers() {
    for (int y = 0; y < boardH; y++) {
        for (int x = 0; x < boardW; x++) {
            if (board[y][x].mine) continue;
            int count = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (inBounds(nx, ny) && board[ny][nx].mine) count++;
                }
            }
            board[y][x].neighborMines = count;
        }
    }
}

static void generateBoardForFirstTap(int safeX, int safeY) {
    clearBoard();
    reseedRng(safeX, safeY);
    placeMines(safeX, safeY);
    calculateNumbers();
    firstTap = false;
}

static void revealAllMines() {
    for (int y = 0; y < boardH; y++) for (int x = 0; x < boardW; x++) if (board[y][x].mine) board[y][x].revealed = true;
}

static void revealTile(int x, int y) {
    if (!inBounds(x, y)) return;
    Cell &cell = board[y][x];
    if (cell.revealed || cell.flagged) return;
    cell.revealed = true;
    if (cell.mine) { gameOver = true; revealAllMines(); return; }
    if (cell.neighborMines == 0) {
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (dx || dy) revealTile(x + dx, y + dy);
    }
}

static void toggleFlag(int x, int y) {
    if (!inBounds(x, y)) return;
    Cell &cell = board[y][x];
    if (cell.revealed) return;
    cell.flagged = !cell.flagged;
    flagsPlaced += cell.flagged ? 1 : -1;
    playFlagSound();
}

static void checkWin() {
    if (gameOver) return;
    for (int y = 0; y < boardH; y++) for (int x = 0; x < boardW; x++) if (!board[y][x].mine && !board[y][x].revealed) return;
    won = true;
    gameOver = true;
}

static void setDifficulty(int index) {
    difficultyIndex = index;
    boardW = difficulties[index].w;
    boardH = difficulties[index].h;
    mineCount = difficulties[index].mines;
}

static void newGame() {
    setDifficulty(difficultyIndex);
    clearBoard();
    gameOver = false; won = false; firstTap = true; flagsPlaced = 0;
    camX = 0; camY = 0; cursorX = 0; cursorY = 0; clampCamera(); ensureCursorVisible();
    touchTracking = false; isDragging = false; longTapTriggered = false; touchHeldFrames = 0;
    moveSfxCooldownFrames = 0;
}

static bool screenToTile(int px, int py, int &tx, int &ty) {
    if (py < VIEW_Y || py >= VIEW_Y + VIEW_H) return false;
    int boardPxW = boardW * TILE + BOARD_FRAME * 2;
    int boardPxH = boardH * TILE + BOARD_FRAME * 2;
    int boardLeft = VIEW_X + (boardPxW < VIEW_W ? (VIEW_W - boardPxW) / 2 : 0);
    int boardTop = VIEW_Y + (boardPxH < VIEW_H ? (VIEW_H - boardPxH) / 2 : 0);
    int wx = px - boardLeft + camX - BOARD_FRAME;
    int wy = py - boardTop + camY - BOARD_FRAME;
    if (wx < 0 || wy < 0) return false;
    tx = wx / TILE;
    ty = wy / TILE;
    return inBounds(tx, ty);
}

// The DS touchscreen is only on the lower/sub screen. The smiley and difficulty
// buttons are drawn on the upper/main screen, so they cannot be touched.
// START/SELECT/X/Y handle new game and difficulty changes instead.
static bool touchOnSmiley(int, int) { return false; }
static int touchDifficulty(int, int) { return -1; }


static void revealCursorTile() {
    if (gameOver) return;
    bool canReveal = inBounds(cursorX, cursorY) && !board[cursorY][cursorX].revealed && !board[cursorY][cursorX].flagged;
    if (firstTap) generateBoardForFirstTap(cursorX, cursorY);
    revealTile(cursorX, cursorY);
    if (canReveal) playRevealSound();
    checkWin();
}

static void moveCursor(int dx, int dy) {
    int oldX = cursorX;
    int oldY = cursorY;
    cursorX += dx;
    cursorY += dy;
    if (cursorX < 0) cursorX = 0;
    if (cursorY < 0) cursorY = 0;
    if (cursorX >= boardW) cursorX = boardW - 1;
    if (cursorY >= boardH) cursorY = boardH - 1;
    if (cursorX != oldX || cursorY != oldY) playMoveSound();
    ensureCursorVisible();
}


int main(void) {
    powerOn(POWER_ALL_2D);
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    irqEnable(IRQ_VBLANK);

    // Make sure libnds keypad state starts clean before the game loop.
    scanKeys();
    int mainBg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int subBg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    vramMain = (u16*)bgGetGfxPtr(mainBg);
    vramSub = (u16*)bgGetGfxPtr(subBg);
    fbCurrent = backBufferSub;

    rngState = mixEntropy(0xC0FFEEu ^ ((u32)REG_VCOUNT << 16));
    newGame();
    drawGame(); presentFrame();


    int leftHeldFrames = 0;
    int rightHeldFrames = 0;
    int upHeldFrames = 0;
    int downHeldFrames = 0;

    while (1) {
        scanKeys();
        int down = keysDown();
        int held = keysHeld();

        if (down & KEY_START) { newGame(); drawGame(); presentFrame(); }
        if (down & KEY_SELECT) { setDifficulty((difficultyIndex + 1) % 3); newGame(); drawGame(); presentFrame(); }

        auto shouldRepeat = [&](int key, int &counter) -> bool {
            if (!(held & key)) {
                counter = 0;
                return false;
            }

            counter++;

            if (down & key) return true;
            if (counter <= KEY_REPEAT_DELAY) return false;
            return ((counter - KEY_REPEAT_DELAY) % KEY_REPEAT_RATE) == 0;
        };

        bool buttonChanged = false;
        if (shouldRepeat(KEY_LEFT, leftHeldFrames))  { moveCursor(-cursorStepForHold(leftHeldFrames), 0); buttonChanged = true; }
        if (shouldRepeat(KEY_RIGHT, rightHeldFrames)) { moveCursor(cursorStepForHold(rightHeldFrames), 0); buttonChanged = true; }
        if (shouldRepeat(KEY_UP, upHeldFrames))    { moveCursor(0, -cursorStepForHold(upHeldFrames)); buttonChanged = true; }
        if (shouldRepeat(KEY_DOWN, downHeldFrames))  { moveCursor(0, cursorStepForHold(downHeldFrames)); buttonChanged = true; }

        if (down & KEY_A)     { revealCursorTile(); buttonChanged = true; }
        if (down & KEY_B)     { if (!gameOver) toggleFlag(cursorX, cursorY); buttonChanged = true; }
        if (down & KEY_X)     { setDifficulty((difficultyIndex + 1) % 3); newGame(); buttonChanged = true; }
        if (down & KEY_Y)     { setDifficulty((difficultyIndex + 2) % 3); newGame(); buttonChanged = true; }

        if (held & KEY_L)     { camX -= PAN_SPEED; clampCamera(); buttonChanged = true; }
        if (held & KEY_R)     { camX += PAN_SPEED; clampCamera(); buttonChanged = true; }

        if (buttonChanged) { drawGame(); presentFrame(); }

        touchPosition touch;
        touchRead(&touch);

        bool sharedTouchHeld = false;
        if (readSharedArm7Touch(touch, sharedTouchHeld)) {
            if (sharedTouchHeld) held |= KEY_TOUCH;
            else held &= ~KEY_TOUCH;
        }

        // DraStic builds on some handhelds report KEY_TOUCH as held,
        // but do not always generate reliable keysDown()/keysUp() edges.
        // So we start/end touch tracking from held-state transitions too.
        bool touchIsHeld = (held & KEY_TOUCH) != 0;
        bool touchStarted = touchIsHeld && !touchTracking;
        bool touchEnded = !touchIsHeld && touchTracking;

        if (touchStarted) {
            int d = touchDifficulty(touch.px, touch.py);
            if (touchOnSmiley(touch.px, touch.py)) {
                newGame();
                drawGame();
                presentFrame();
            } else if (d >= 0) {
                setDifficulty(d);
                newGame();
                drawGame();
                presentFrame();
            } else {
                touchStartPx = lastTouchPx = touch.px;
                touchStartPy = lastTouchPy = touch.py;
                touchHeldFrames = 0;
                isDragging = false;
                longTapTriggered = false;
                touchTracking = screenToTile(touch.px, touch.py, touchStartTileX, touchStartTileY);
            }
        }

        if (touchTracking && touchIsHeld) {
            int totalDx = touch.px - touchStartPx;
            int totalDy = touch.py - touchStartPy;
            if (!isDragging && (abs(totalDx) > DRAG_THRESHOLD || abs(totalDy) > DRAG_THRESHOLD)) {
                isDragging = true;
            }

            if (isDragging) {
                camX -= touch.px - lastTouchPx;
                camY -= touch.py - lastTouchPy;
                clampCamera();
                lastTouchPx = touch.px;
                lastTouchPy = touch.py;
                drawGame();
                presentFrame();
            } else if (!gameOver) {
                touchHeldFrames++;
                if (!longTapTriggered && touchHeldFrames >= LONG_TAP_FRAMES) {
                    toggleFlag(touchStartTileX, touchStartTileY);
                    longTapTriggered = true;
                    drawGame();
                    presentFrame();
                }
            }
        }

        if (touchEnded) {
            if (!isDragging && !longTapTriggered && !gameOver) {
                int tx, ty;
                if (screenToTile(touchStartPx, touchStartPy, tx, ty)) {
                    bool canReveal = inBounds(tx, ty) && !board[ty][tx].revealed && !board[ty][tx].flagged;
                    if (firstTap) generateBoardForFirstTap(tx, ty);
                    revealTile(tx, ty);
                    if (canReveal) playRevealSound();
                    checkWin();
                    drawGame();
                    presentFrame();
                }
            }
            touchTracking = false;
            isDragging = false;
            longTapTriggered = false;
            touchHeldFrames = 0;
        }

        waitFrame();
    }
}