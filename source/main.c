// ROBLOX 2013 (fan tribute) for the New Nintendo 3DS
// Software-rendered 3D on the top screen, 2013-style Game Menu on the bottom.
//
// Controls
//   Circle Pad : move            C-Stick : camera
//   A          : jump            B (twice): reset character
//   L / R      : zoom camera     START   : pause
//   Touch      : bottom-screen Game Menu
//
// Only depends on libctru (no citro3d / shaders), so it is easy to build.

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TW 400
#define TH 240
#define BW 320
#define BH 240
#define FOCAL 285.0f
#define NEARZ 0.25f
#define PI_F 3.14159265f
#define GRAV 196.2f
#define JUMP_POWER 50.0f
#define WALK_SPEED 16.0f
#define ACTOR_R 0.8f
#define ACTOR_H 5.0f
#define WORLD_HALF 148.0f
#define MAXBLOCKS 48
#define NACT 1
#define CLOUD_H 400.0f

#define RGB(r, g, b) ((u32)((((u32)(r)) << 16) | (((u32)(g)) << 8) | ((u32)(b))))
#define C_RED RGB(196, 40, 28)
#define C_BLUE RGB(13, 105, 172)
#define C_YELLOW RGB(245, 205, 48)
#define C_DGREEN RGB(40, 127, 71)
#define C_BROWN RGB(105, 64, 40)
#define C_GREY RGB(163, 162, 165)
#define C_ORANGE RGB(218, 133, 65)
#define C_GRASS RGB(75, 151, 75)
#define C_HORIZON RGB(196, 218, 236)
#define C_ZENITH RGB(66, 126, 214)

// ---------------------------------------------------------------------------
// Surfaces and 2D drawing
// ---------------------------------------------------------------------------
typedef struct { u32 *p; int w, h; } Surf;
static u32 topbuf[TW * TH];
static float zbuf[TW * TH];
static u32 botbuf[BW * BH];
static Surf ST = { topbuf, TW, TH };
static Surf SB = { botbuf, BW, BH };

static float frand(float a, float b) { return a + (b - a) * ((float)rand() / (float)RAND_MAX); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float wrapang(float a) { while (a > PI_F) a -= 2 * PI_F; while (a < -PI_F) a += 2 * PI_F; return a; }

static inline u32 mixc(u32 a, u32 b, int t) { // t: 0..256 weight of b
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return RGB(ar + (((br - ar) * t) >> 8), ag + (((bg - ag) * t) >> 8), ab + (((bb - ab) * t) >> 8));
}
static inline u32 shade(u32 c, float k) {
    int r = (int)(((c >> 16) & 255) * k), g = (int)(((c >> 8) & 255) * k), b = (int)((c & 255) * k);
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return RGB(r, g, b);
}
static inline void d_put(Surf *s, int x, int y, u32 c) {
    if ((unsigned)x < (unsigned)s->w && (unsigned)y < (unsigned)s->h) s->p[y * s->w + x] = c;
}
static void d_rect(Surf *s, int x, int y, int w, int h, u32 c) {
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x1 > s->w) x1 = s->w; if (y1 > s->h) y1 = s->h;
    for (int j = y; j < y1; j++) { u32 *r = s->p + j * s->w; for (int i = x; i < x1; i++) r[i] = c; }
}
static void d_rect_a(Surf *s, int x, int y, int w, int h, u32 c, int a) {
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x1 > s->w) x1 = s->w; if (y1 > s->h) y1 = s->h;
    for (int j = y; j < y1; j++) { u32 *r = s->p + j * s->w; for (int i = x; i < x1; i++) r[i] = mixc(r[i], c, a); }
}
static void d_frame(Surf *s, int x, int y, int w, int h, u32 c, int t) {
    d_rect(s, x, y, w, t, c); d_rect(s, x, y + h - t, w, t, c);
    d_rect(s, x, y, t, h, c); d_rect(s, x + w - t, y, t, h, c);
}

// 5x7 font in an 8-row cell (rows 0-6 caps/digits, lowercase uses rows 2-7)
typedef struct { char c; u8 r[8]; } Glyph;
#define GL(ch, a, b, c, d, e, f, g, h) { ch, { a, b, c, d, e, f, g, h } }
static const Glyph FONT[] = {
    GL(' ', 0, 0, 0, 0, 0, 0, 0, 0),
    GL('!', 4, 4, 4, 4, 4, 0, 4, 0), GL('\'', 12, 4, 8, 0, 0, 0, 0, 0),
    GL('(', 2, 4, 8, 8, 8, 4, 2, 0), GL(')', 8, 4, 2, 2, 2, 4, 8, 0),
    GL('+', 0, 4, 4, 31, 4, 4, 0, 0), GL(',', 0, 0, 0, 0, 12, 4, 8, 0),
    GL('-', 0, 0, 0, 31, 0, 0, 0, 0), GL('.', 0, 0, 0, 0, 0, 12, 12, 0),
    GL('/', 1, 1, 2, 4, 8, 16, 16, 0), GL(':', 0, 12, 12, 0, 12, 12, 0, 0),
    GL('?', 14, 17, 1, 2, 4, 0, 4, 0), GL('_', 0, 0, 0, 0, 0, 0, 31, 0),
    GL('0', 14, 17, 19, 21, 25, 17, 14, 0), GL('1', 4, 12, 4, 4, 4, 4, 14, 0),
    GL('2', 14, 17, 1, 2, 4, 8, 31, 0), GL('3', 31, 2, 4, 2, 1, 17, 14, 0),
    GL('4', 2, 6, 10, 18, 31, 2, 2, 0), GL('5', 31, 16, 30, 1, 1, 17, 14, 0),
    GL('6', 6, 8, 16, 30, 17, 17, 14, 0), GL('7', 31, 1, 2, 4, 8, 8, 8, 0),
    GL('8', 14, 17, 17, 14, 17, 17, 14, 0), GL('9', 14, 17, 17, 15, 1, 2, 12, 0),
    GL('A', 14, 17, 17, 31, 17, 17, 17, 0), GL('B', 30, 17, 17, 30, 17, 17, 30, 0),
    GL('C', 14, 17, 16, 16, 16, 17, 14, 0), GL('D', 30, 17, 17, 17, 17, 17, 30, 0),
    GL('E', 31, 16, 16, 30, 16, 16, 31, 0), GL('F', 31, 16, 16, 30, 16, 16, 16, 0),
    GL('G', 14, 17, 16, 23, 17, 17, 15, 0), GL('H', 17, 17, 17, 31, 17, 17, 17, 0),
    GL('I', 14, 4, 4, 4, 4, 4, 14, 0), GL('J', 7, 2, 2, 2, 2, 18, 12, 0),
    GL('K', 17, 18, 20, 24, 20, 18, 17, 0), GL('L', 16, 16, 16, 16, 16, 16, 31, 0),
    GL('M', 17, 27, 21, 21, 17, 17, 17, 0), GL('N', 17, 25, 21, 19, 17, 17, 17, 0),
    GL('O', 14, 17, 17, 17, 17, 17, 14, 0), GL('P', 30, 17, 17, 30, 16, 16, 16, 0),
    GL('Q', 14, 17, 17, 17, 21, 18, 13, 0), GL('R', 30, 17, 17, 30, 20, 18, 17, 0),
    GL('S', 15, 16, 16, 14, 1, 1, 30, 0), GL('T', 31, 4, 4, 4, 4, 4, 4, 0),
    GL('U', 17, 17, 17, 17, 17, 17, 14, 0), GL('V', 17, 17, 17, 17, 17, 10, 4, 0),
    GL('W', 17, 17, 17, 21, 21, 21, 10, 0), GL('X', 17, 17, 10, 4, 10, 17, 17, 0),
    GL('Y', 17, 17, 10, 4, 4, 4, 4, 0), GL('Z', 31, 1, 2, 4, 8, 16, 31, 0),
    GL('a', 0, 0, 14, 1, 15, 17, 15, 0), GL('b', 16, 16, 22, 25, 17, 17, 30, 0),
    GL('c', 0, 0, 14, 16, 16, 17, 14, 0), GL('d', 1, 1, 13, 19, 17, 17, 15, 0),
    GL('e', 0, 0, 14, 17, 31, 16, 14, 0), GL('f', 6, 9, 8, 28, 8, 8, 8, 0),
    GL('g', 0, 0, 15, 17, 17, 15, 1, 14), GL('h', 16, 16, 22, 25, 17, 17, 17, 0),
    GL('i', 4, 0, 12, 4, 4, 4, 14, 0), GL('j', 2, 0, 6, 2, 2, 2, 18, 12),
    GL('k', 16, 16, 18, 20, 24, 20, 18, 0), GL('l', 12, 4, 4, 4, 4, 4, 14, 0),
    GL('m', 0, 0, 26, 21, 21, 21, 21, 0), GL('n', 0, 0, 22, 25, 17, 17, 17, 0),
    GL('o', 0, 0, 14, 17, 17, 17, 14, 0), GL('p', 0, 0, 30, 17, 17, 30, 16, 16),
    GL('q', 0, 0, 15, 17, 17, 15, 1, 1), GL('r', 0, 0, 22, 25, 16, 16, 16, 0),
    GL('s', 0, 0, 15, 16, 14, 1, 30, 0), GL('t', 8, 8, 28, 8, 8, 9, 6, 0),
    GL('u', 0, 0, 17, 17, 17, 19, 13, 0), GL('v', 0, 0, 17, 17, 17, 10, 4, 0),
    GL('w', 0, 0, 17, 17, 21, 21, 10, 0), GL('x', 0, 0, 17, 10, 4, 10, 17, 0),
    GL('y', 0, 0, 17, 17, 17, 15, 1, 14), GL('z', 0, 0, 31, 2, 4, 8, 31, 0),
};
static const u8 *glyph(char c) {
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++) if (FONT[i].c == c) return FONT[i].r;
    return NULL;
}
static void d_char(Surf *s, int x, int y, char c, int sc, u32 col) {
    const u8 *g = glyph(c);
    if (!g) return;
    for (int r = 0; r < 8; r++) {
        u8 bits = g[r];
        for (int b = 0; b < 5; b++) {
            if (bits & (0x10 >> b)) {
                if (sc == 1) d_put(s, x + b, y + r, col);
                else d_rect(s, x + b * sc, y + r * sc, sc, sc, col);
            }
        }
    }
}
static int text_w(const char *t, int sc) { return (int)strlen(t) * 6 * sc; }
static void d_text(Surf *s, int x, int y, const char *t, int sc, u32 col) {
    for (; *t; t++, x += 6 * sc) d_char(s, x, y, *t, sc, col);
}
static void d_text_sh(Surf *s, int x, int y, const char *t, int sc, u32 col) {
    d_text(s, x + sc, y + sc, t, sc, RGB(0, 0, 0)); d_text(s, x, y, t, sc, col);
}
static void d_text_c(Surf *s, int cx, int y, const char *t, int sc, u32 col) {
    d_text(s, cx - text_w(t, sc) / 2, y, t, sc, col);
}


// ---------------------------------------------------------------------------
// Simple generic fixed-width font for bottom screen
// ---------------------------------------------------------------------------
// Simple generic fixed-width bitmap font (6 wide x 10 high)
typedef struct { char c; u8 r[10]; } GlyphSimple;
static const GlyphSimple FONT_SIMPLE[] = {
    { ' ', { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { '!', { 12, 12, 12, 12, 12, 0, 12, 12, 0, 0 } },
    { '\'', { 24, 12, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { '(', { 12, 24, 48, 48, 48, 48, 24, 12, 0, 0 } },
    { ')', { 48, 24, 12, 12, 12, 12, 24, 48, 0, 0 } },
    { '+', { 0, 12, 12, 63, 12, 12, 0, 0, 0, 0 } },
    { ',', { 0, 0, 0, 0, 0, 0, 12, 12, 24, 0 } },
    { '-', { 0, 0, 0, 0, 63, 0, 0, 0, 0, 0 } },
    { '.', { 0, 0, 0, 0, 0, 0, 12, 12, 0, 0 } },
    { '/', { 3, 3, 6, 12, 24, 48, 48, 96, 0, 0 } },
    { '0', { 30, 51, 51, 51, 51, 51, 51, 30, 0, 0 } },
    { '1', { 12, 28, 12, 12, 12, 12, 12, 30, 0, 0 } },
    { '2', { 30, 51, 3, 6, 12, 24, 48, 63, 0, 0 } },
    { '3', { 30, 51, 3, 14, 3, 3, 51, 30, 0, 0 } },
    { '4', { 6, 14, 22, 38, 63, 6, 6, 6, 0, 0 } },
    { '5', { 63, 48, 62, 3, 3, 3, 51, 30, 0, 0 } },
    { '6', { 14, 24, 48, 62, 51, 51, 51, 30, 0, 0 } },
    { '7', { 63, 3, 6, 12, 24, 24, 24, 24, 0, 0 } },
    { '8', { 30, 51, 51, 30, 51, 51, 51, 30, 0, 0 } },
    { '9', { 30, 51, 51, 31, 3, 6, 12, 24, 0, 0 } },
    { ':', { 0, 12, 12, 0, 0, 12, 12, 0, 0, 0 } },
    { '?', { 30, 51, 3, 6, 12, 0, 12, 12, 0, 0 } },
    { 'A', { 30, 51, 51, 51, 63, 51, 51, 51, 51, 0 } },
    { 'B', { 62, 51, 51, 62, 51, 51, 51, 62, 0, 0 } },
    { 'C', { 30, 51, 48, 48, 48, 48, 51, 30, 0, 0 } },
    { 'D', { 60, 54, 51, 51, 51, 51, 54, 60, 0, 0 } },
    { 'E', { 63, 48, 48, 60, 48, 48, 48, 63, 0, 0 } },
    { 'F', { 63, 48, 48, 60, 48, 48, 48, 48, 0, 0 } },
    { 'G', { 30, 51, 48, 48, 55, 51, 51, 30, 0, 0 } },
    { 'H', { 51, 51, 51, 63, 51, 51, 51, 51, 0, 0 } },
    { 'I', { 30, 12, 12, 12, 12, 12, 12, 30, 0, 0 } },
    { 'J', { 15, 6, 6, 6, 6, 38, 38, 28, 0, 0 } },
    { 'K', { 51, 54, 60, 56, 60, 54, 51, 51, 0, 0 } },
    { 'L', { 48, 48, 48, 48, 48, 48, 48, 63, 0, 0 } },
    { 'M', { 51, 63, 63, 51, 51, 51, 51, 51, 0, 0 } },
    { 'N', { 51, 51, 59, 63, 55, 51, 51, 51, 0, 0 } },
    { 'O', { 30, 51, 51, 51, 51, 51, 51, 30, 0, 0 } },
    { 'P', { 62, 51, 51, 62, 48, 48, 48, 48, 0, 0 } },
    { 'Q', { 30, 51, 51, 51, 51, 55, 30, 3, 0, 0 } },
    { 'R', { 62, 51, 51, 62, 60, 54, 51, 51, 0, 0 } },
    { 'S', { 31, 48, 48, 30, 3, 3, 51, 30, 0, 0 } },
    { 'T', { 63, 12, 12, 12, 12, 12, 12, 12, 0, 0 } },
    { 'U', { 51, 51, 51, 51, 51, 51, 51, 30, 0, 0 } },
    { 'V', { 51, 51, 51, 51, 51, 30, 12, 12, 0, 0 } },
    { 'W', { 51, 51, 51, 51, 63, 63, 51, 51, 0, 0 } },
    { 'X', { 51, 51, 30, 12, 30, 51, 51, 51, 0, 0 } },
    { 'Y', { 51, 51, 30, 12, 12, 12, 12, 12, 0, 0 } },
    { 'Z', { 63, 3, 6, 12, 24, 48, 48, 63, 0, 0 } },
    { '_', { 0, 0, 0, 0, 0, 0, 0, 0, 63, 0 } },
    { 'a', { 0, 0, 30, 3, 31, 51, 51, 31, 0, 0 } },
    { 'b', { 48, 48, 62, 51, 51, 51, 51, 62, 0, 0 } },
    { 'c', { 0, 0, 30, 48, 48, 48, 51, 30, 0, 0 } },
    { 'd', { 3, 3, 31, 51, 51, 51, 51, 31, 0, 0 } },
    { 'e', { 0, 0, 30, 51, 63, 48, 51, 30, 0, 0 } },
    { 'f', { 14, 24, 60, 24, 24, 24, 24, 24, 0, 0 } },
    { 'g', { 0, 0, 31, 51, 51, 31, 3, 30, 0, 0 } },
    { 'h', { 48, 48, 62, 51, 51, 51, 51, 51, 0, 0 } },
    { 'i', { 12, 0, 28, 12, 12, 12, 12, 30, 0, 0 } },
    { 'j', { 6, 0, 14, 6, 6, 6, 38, 28, 0, 0 } },
    { 'k', { 48, 48, 51, 54, 60, 54, 51, 51, 0, 0 } },
    { 'l', { 28, 12, 12, 12, 12, 12, 12, 30, 0, 0 } },
    { 'm', { 0, 0, 54, 63, 51, 51, 51, 51, 0, 0 } },
    { 'n', { 0, 0, 62, 51, 51, 51, 51, 51, 0, 0 } },
    { 'o', { 0, 0, 30, 51, 51, 51, 51, 30, 0, 0 } },
    { 'p', { 0, 0, 62, 51, 51, 62, 48, 48, 0, 0 } },
    { 'q', { 0, 0, 31, 51, 51, 31, 3, 3, 0, 0 } },
    { 'r', { 0, 0, 62, 51, 48, 48, 48, 48, 0, 0 } },
    { 's', { 0, 0, 31, 48, 30, 3, 51, 30, 0, 0 } },
    { 't', { 24, 24, 60, 24, 24, 24, 24, 14, 0, 0 } },
    { 'u', { 0, 0, 51, 51, 51, 51, 51, 31, 0, 0 } },
    { 'v', { 0, 0, 51, 51, 51, 30, 12, 12, 0, 0 } },
    { 'w', { 0, 0, 51, 51, 51, 63, 63, 51, 0, 0 } },
    { 'x', { 0, 0, 51, 30, 12, 30, 51, 51, 0, 0 } },
    { 'y', { 0, 0, 51, 51, 51, 31, 3, 30, 0, 0 } },
    { 'z', { 0, 0, 63, 6, 12, 24, 48, 63, 0, 0 } },
};

static const u8 *glyph_s(char c) {
    for (size_t i = 0; i < sizeof(FONT_SIMPLE) / sizeof(FONT_SIMPLE[0]); i++)
        if (FONT_SIMPLE[i].c == c) return FONT_SIMPLE[i].r;
    return NULL;
}
static void d_char_s(Surf *s, int x, int y, char c, int sc, u32 col) {
    const u8 *g = glyph_s(c);
    if (!g) return;
    for (int r = 0; r < 10; r++) {
        u8 bits = g[r];
        for (int b = 0; b < 6; b++) {
            if (bits & (0x20 >> b)) {
                if (sc == 1) d_put(s, x + b, y + r, col);
                else d_rect(s, x + b * sc, y + r * sc, sc, sc, col);
            }
        }
    }
}
static int text_w_s(const char *t, int sc) { return (int)strlen(t) * 7 * sc; }
static void d_text_s(Surf *s, int x, int y, const char *t, int sc, u32 col) {
    for (; *t; t++, x += 7 * sc) d_char_s(s, x, y, *t, sc, col);
}
static void d_text_c_s(Surf *s, int cx, int y, const char *t, int sc, u32 col) {
    d_text_s(s, cx - text_w_s(t, sc) / 2, y, t, sc, col);
}

// ---------------------------------------------------------------------------
// Camera + 3D rasteriser (software, z-buffered, flat shaded)
// ---------------------------------------------------------------------------
static float camp[3], camf[3], camr[3], camu[3];
static float yaw = 0.0f, pitch = -0.2f, camdist = 14.0f;
static u8 cloudtex[256 * 256];

static void cam_update(const float tgt[3]) {
    float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);
    camf[0] = cp * sy; camf[1] = sp; camf[2] = cp * cy;
    camr[0] = cy; camr[1] = 0; camr[2] = -sy;
    camu[0] = -sp * sy; camu[1] = cp; camu[2] = -sp * cy;
    for (int i = 0; i < 3; i++) camp[i] = tgt[i] - camf[i] * camdist;
    if (camp[1] < 0.7f) camp[1] = 0.7f;
}
static void to_cam(const float *p, float *o) {
    float dx = p[0] - camp[0], dy = p[1] - camp[1], dz = p[2] - camp[2];
    o[0] = dx * camr[0] + dy * camr[1] + dz * camr[2];
    o[1] = dx * camu[0] + dy * camu[1] + dz * camu[2];
    o[2] = dx * camf[0] + dy * camf[1] + dz * camf[2];
}
static int proj(const float *p, int *sx, int *sy, float *z) {
    float c[3]; to_cam(p, c);
    if (c[2] < NEARZ) return 0;
    *sx = (int)(TW / 2 + c[0] / c[2] * FOCAL); *sy = (int)(TH / 2 - c[1] / c[2] * FOCAL); *z = c[2];
    return 1;
}

static void raster_tri(float x0, float y0, float z0, float x1, float y1, float z1,
                       float x2, float y2, float z2, u32 col) {
    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (fabsf(area) < 0.02f) return;
    float minx = fminf(x0, fminf(x1, x2)), maxx = fmaxf(x0, fmaxf(x1, x2));
    float miny = fminf(y0, fminf(y1, y2)), maxy = fmaxf(y0, fmaxf(y1, y2));
    if (maxx < 0 || maxy < 0 || minx > TW - 1 || miny > TH - 1) return;
    int ix0 = (int)fmaxf(minx, 0), ix1 = (int)fminf(maxx, TW - 1);
    int iy0 = (int)fmaxf(miny, 0), iy1 = (int)fminf(maxy, TH - 1);
    float fl = 1.0f;
    if (area < 0) { fl = -1.0f; area = -area; }
    float px = ix0 + 0.5f, py = iy0 + 0.5f;
    float w0 = fl * ((x2 - x1) * (py - y1) - (y2 - y1) * (px - x1));
    float w1 = fl * ((x0 - x2) * (py - y2) - (y0 - y2) * (px - x2));
    float w2 = fl * ((x1 - x0) * (py - y0) - (y1 - y0) * (px - x0));
    float d0x = -fl * (y2 - y1), d0y = fl * (x2 - x1);
    float d1x = -fl * (y0 - y2), d1y = fl * (x0 - x2);
    float d2x = -fl * (y1 - y0), d2y = fl * (x1 - x0);
    float inv = 1.0f / area;
    for (int y = iy0; y <= iy1; y++) {
        float a0 = w0, a1 = w1, a2 = w2;
        u32 *row = topbuf + y * TW; float *zr = zbuf + y * TW;
        for (int x = ix0; x <= ix1; x++) {
            if (a0 >= 0 && a1 >= 0 && a2 >= 0) {
                float z = (a0 * z0 + a1 * z1 + a2 * z2) * inv;
                if (z > zr[x]) { zr[x] = z; row[x] = col; }
            }
            a0 += d0x; a1 += d1x; a2 += d2x;
        }
        w0 += d0y; w1 += d1y; w2 += d2y;
    }
}

static void draw_poly(float v[][3], int n, u32 col) {
    float o[10][3]; int m = 0;
    for (int i = 0; i < n; i++) {
        const float *a = v[i], *b = v[(i + 1) % n];
        int ia = a[2] >= NEARZ, ib = b[2] >= NEARZ;
        if (ia) { o[m][0] = a[0]; o[m][1] = a[1]; o[m][2] = a[2]; m++; }
        if (ia != ib) {
            float t = (NEARZ - a[2]) / (b[2] - a[2]);
            o[m][0] = a[0] + (b[0] - a[0]) * t; o[m][1] = a[1] + (b[1] - a[1]) * t; o[m][2] = NEARZ; m++;
        }
    }
    if (m < 3) return;
    float sx[10], sy[10], iz[10];
    for (int i = 0; i < m; i++) {
        sx[i] = TW / 2 + o[i][0] / o[i][2] * FOCAL; sy[i] = TH / 2 - o[i][1] / o[i][2] * FOCAL; iz[i] = 1.0f / o[i][2];
    }
    for (int i = 1; i < m - 1; i++) raster_tri(sx[0], sy[0], iz[0], sx[i], sy[i], iz[i], sx[i + 1], sy[i + 1], iz[i + 1], col);
}

typedef struct { float c[3], ax[3], ay[3], az[3], h[3]; u32 col; } OBox;
static const float LDIR[3] = { 0.26f, 0.86f, 0.43f };

static void draw_obox(const OBox *b) {
    float cc[3]; to_cam(b->c, cc);
    float rad = sqrtf(b->h[0] * b->h[0] + b->h[1] * b->h[1] + b->h[2] * b->h[2]);
    if (cc[2] + rad < NEARZ) return;
    const float *axes[3] = { b->ax, b->ay, b->az };
    for (int a = 0; a < 3; a++) {
        int b1 = (a + 1) % 3, b2 = (a + 2) % 3;
        for (int s = -1; s <= 1; s += 2) {
            float n[3], fc[3];
            for (int i = 0; i < 3; i++) { n[i] = axes[a][i] * s; fc[i] = b->c[i] + n[i] * b->h[a]; }
            float vd = n[0] * (camp[0] - fc[0]) + n[1] * (camp[1] - fc[1]) + n[2] * (camp[2] - fc[2]);
            if (vd <= 0) continue;
            float l = n[0] * LDIR[0] + n[1] * LDIR[1] + n[2] * LDIR[2];
            u32 col = shade(b->col, 0.45f + 0.55f * (l > 0 ? l : 0));
            static const int sg[4][2] = { { 1, 1 }, { 1, -1 }, { -1, -1 }, { -1, 1 } };
            float cv[4][3];
            for (int k = 0; k < 4; k++) {
                float w[3];
                for (int i = 0; i < 3; i++) w[i] = fc[i] + axes[b1][i] * b->h[b1] * sg[k][0] + axes[b2][i] * b->h[b2] * sg[k][1];
                to_cam(w, cv[k]);
            }
            draw_poly(cv, 4, col);
        }
    }
}
static void draw_box(const float c[3], const float ax[3], const float ay[3], const float az[3],
                     float hx, float hy, float hz, u32 col) {
    OBox b;
    memcpy(b.c, c, 12); memcpy(b.ax, ax, 12); memcpy(b.ay, ay, 12); memcpy(b.az, az, 12);
    b.h[0] = hx; b.h[1] = hy; b.h[2] = hz; b.col = col;
    draw_obox(&b);
}

// ---------------------------------------------------------------------------
// Sky + baseplate (per-row ray casting: very cheap)
// ---------------------------------------------------------------------------
static int opt_studs = 1;

static float vnoise(const u8 *lat, int n, float x, float y) {
    int xi = (int)x, yi = (int)y; float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    int x1 = (xi + 1) % n, y1 = (yi + 1) % n; xi %= n; yi %= n;
    float a = lat[yi * n + xi], b = lat[yi * n + x1], c = lat[y1 * n + xi], d = lat[y1 * n + x1];
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
}
static void gen_clouds(void) {
    static u8 l8[64], l16[256], l32[1024];
    for (int i = 0; i < 64; i++) l8[i] = rand() & 255;
    for (int i = 0; i < 256; i++) l16[i] = rand() & 255;
    for (int i = 0; i < 1024; i++) l32[i] = rand() & 255;
    for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) {
        float v = 0.55f * vnoise(l8, 8, x * 8 / 256.0f, y * 8 / 256.0f)
                + 0.30f * vnoise(l16, 16, x * 16 / 256.0f, y * 16 / 256.0f)
                + 0.15f * vnoise(l32, 32, x * 32 / 256.0f, y * 32 / 256.0f);
        cloudtex[y * 256 + x] = (u8)v;
    }
}

static void render_bg(void) {
    static float colA[TW]; static int init = 0;
    if (!init) { for (int x = 0; x < TW; x++) colA[x] = (x + 0.5f - TW / 2) / FOCAL; init = 1; }
    for (int y = 0; y < TH; y++) {
        float b = -((y + 0.5f) - TH / 2) / FOCAL;
        float dy = camf[1] + b * camu[1];
        float bx = camf[0] + b * camu[0], bz = camf[2] + b * camu[2];
        u32 *row = topbuf + y * TW; float *zr = zbuf + y * TW;
        if (dy < -0.0004f) {
            float t = -camp[1] / dy;
            float X0 = camp[0] + t * bx, Z0 = camp[2] + t * bz, sx = t * camr[0], sz = t * camr[2];
            float invz = 1.0f / t;
            int fog = (int)(clampf((t - 60.0f) / 220.0f, 0, 1) * 256);
            static const float K[3] = { 1.10f, 0.96f, 0.78f };
            u32 cols[2][3];
            for (int m = 0; m < 2; m++) for (int k = 0; k < 3; k++)
                cols[m][k] = mixc(shade(m ? C_GREY : C_GRASS, K[k]), C_HORIZON, fog);
            int studs = opt_studs && t < 70.0f;
            for (int x = 0; x < TW; x++) {
                float X = X0 + colA[x] * sx, Z = Z0 + colA[x] * sz;
                zr[x] = invz;
                if (X < -150 || X > 150 || Z < -150 || Z > 150) { row[x] = C_HORIZON; continue; }
                int mat = (fabsf(X) < 4.0f || fabsf(Z) < 4.0f);
                int sh = 1;
                if (studs) {
                    float fx = X + 512.0f, fz = Z + 512.0f;
                    fx -= (int)fx; fz -= (int)fz;
                    float dx = fx - 0.5f, dz = fz - 0.5f, d = dx * dx + dz * dz;
                    sh = d < 0.09f ? 0 : (d < 0.17f ? 2 : 1);
                }
                row[x] = cols[mat][sh];
            }
        } else {
            u32 sky = mixc(C_HORIZON, C_ZENITH, (int)(clampf(dy * 1.8f, 0, 1) * 256));
            if (dy > 0.02f) {
                float t2 = (CLOUD_H - camp[1]) / dy;
                float X0 = camp[0] + t2 * bx, Z0 = camp[2] + t2 * bz, sx = t2 * camr[0], sz = t2 * camr[2];
                int cf = (int)(clampf((dy - 0.02f) * 6.0f, 0, 1) * 256);
                for (int x = 0; x < TW; x++) {
                    float X = X0 + colA[x] * sx, Z = Z0 + colA[x] * sz;
                    int idx = (((int)(X * 0.06f + 4096.0f)) & 255) | ((((int)(Z * 0.06f + 4096.0f)) & 255) << 8);
                    int cov = ((int)cloudtex[idx] - 118) * 3;
                    cov = cov < 0 ? 0 : (cov > 256 ? 256 : cov);
                    row[x] = cov ? mixc(sky, RGB(255, 255, 255), (cov * cf) >> 8) : sky;
                    zr[x] = 0.0f;
                }
            } else {
                for (int x = 0; x < TW; x++) { row[x] = sky; zr[x] = 0.0f; }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// World + actors
// ---------------------------------------------------------------------------
typedef struct { float x, y, z, sx, sy, sz; u32 col; int kind; } Block; // kind 0 solid, 1 kill, 2 hurt
static Block blocks[MAXBLOCKS];
static int nblocks = 0;
static void addb(float x, float y, float z, float sx, float sy, float sz, u32 col, int kind) {
    if (nblocks < MAXBLOCKS) { Block b = { x, y, z, sx, sy, sz, col, kind }; blocks[nblocks++] = b; }
}
static void build_world(void) {
    addb(0, 0.5f, 0, 8, 1, 8, C_GREY, 0);                       // SpawnLocation
    for (int i = 0; i < 8; i++)                                 // stairs (1 stud rise)
        addb(16 + 3 * i, (i + 1) * 0.5f, 14, 3, (float)(i + 1), 8, (i & 1) ? C_YELLOW : C_ORANGE, 0);
    addb(42, 4, 14, 8, 8, 8, C_GREY, 0);
    addb(-24, 5, 18, 10, 10, 10, C_BLUE, 0);
    addb(-12, 2, 26, 4, 4, 4, C_YELLOW, 0);
    addb(-18, 1.5f, 8, 6, 3, 6, C_DGREEN, 0);
    addb(0, 0.25f, 34, 14, 0.5f, 14, C_RED, 1);                // kill brick
    addb(20, 0.25f, -18, 8, 0.5f, 8, RGB(255, 128, 0), 2);     // damage brick
    static const float tp[4][2] = { { -30, -20 }, { 26, -30 }, { -18, 44 }, { 42, -6 } };
    for (int i = 0; i < 4; i++) {
        addb(tp[i][0], 4, tp[i][1], 2, 8, 2, C_BROWN, 0);
        addb(tp[i][0], 10.5f, tp[i][1], 8, 5, 8, C_DGREEN, 0);
    }
    addb(0, 1.5f, -60, 40, 3, 2, C_GREY, 0);
}

typedef struct {
    float x, y, z, vy, theta, phase, moveamt, hp;
    int ground, npc;
    u32 cTorso, cLegs;
    const char *name;
    float tx, tz, wait, stuck, hurtcd, sayT, nextSay;
    char msg[56];
} Actor;
static Actor act[NACT];

static int overlapb(const Actor *a, const Block *b, float e) {
    return a->x + ACTOR_R + e > b->x - b->sx * .5f && a->x - ACTOR_R - e < b->x + b->sx * .5f &&
           a->z + ACTOR_R + e > b->z - b->sz * .5f && a->z - ACTOR_R - e < b->z + b->sz * .5f &&
           a->y + ACTOR_H + e > b->y - b->sy * .5f && a->y - e < b->y + b->sy * .5f;
}

static void move_actor(Actor *a, float dx, float dz, float dt) {
    a->x += dx;
    for (int i = 0; i < nblocks; i++) {
        const Block *b = &blocks[i];
        if (!overlapb(a, b, 0)) continue;
        float top = b->y + b->sy * .5f;
        if (a->ground && top - a->y <= 1.05f) { a->y = top; continue; }
        a->x = dx > 0 ? b->x - b->sx * .5f - ACTOR_R : b->x + b->sx * .5f + ACTOR_R;
    }
    a->z += dz;
    for (int i = 0; i < nblocks; i++) {
        const Block *b = &blocks[i];
        if (!overlapb(a, b, 0)) continue;
        float top = b->y + b->sy * .5f;
        if (a->ground && top - a->y <= 1.05f) { a->y = top; continue; }
        a->z = dz > 0 ? b->z - b->sz * .5f - ACTOR_R : b->z + b->sz * .5f + ACTOR_R;
    }
    a->x = clampf(a->x, -WORLD_HALF, WORLD_HALF); a->z = clampf(a->z, -WORLD_HALF, WORLD_HALF);
    a->vy -= GRAV * dt; a->y += a->vy * dt; a->ground = 0;
    if (a->y <= 0) { a->y = 0; a->vy = 0; a->ground = 1; }
    for (int i = 0; i < nblocks; i++) {
        const Block *b = &blocks[i];
        if (!overlapb(a, b, 0)) continue;
        if (a->vy <= 0) { a->y = b->y + b->sy * .5f; a->vy = 0; a->ground = 1; }
        else { a->y = b->y - b->sy * .5f - ACTOR_H; a->vy = 0; }
    }
    // damage bricks
    for (int i = 0; i < nblocks; i++) {
        const Block *b = &blocks[i];
        if (b->kind == 0 || !overlapb(a, b, 0.12f)) continue;
        if (b->kind == 1) a->hp = 0;
        else if (a->hurtcd <= 0) { a->hp -= 25; a->hurtcd = 1.0f; }
    }
}

static void respawn(Actor *a) {
    if (a->npc) { a->x = frand(-6, 8); a->z = frand(4, 10); } else { a->x = 0; a->z = 0; }
    a->y = 1.0f; a->vy = 0; a->hp = 100; a->wait = 0; a->tx = a->x; a->tz = a->z;
}

static void init_actors(void) {
    static const char *names[NACT] = { "Player" };
    static const u32 tc[NACT] = { C_BLUE };
    static const u32 lc[NACT] = { C_DGREEN };
    for (int i = 0; i < NACT; i++) {
        memset(&act[i], 0, sizeof(Actor));
        act[i].npc = i > 0; act[i].name = names[i]; act[i].cTorso = tc[i]; act[i].cLegs = lc[i];
        act[i].nextSay = frand(3, 14); respawn(&act[i]);
    }
}

// ---------------------------------------------------------------------------
// Drawing characters
// ---------------------------------------------------------------------------
static void draw_limb(const Actor *a, const float R[3], const float F[3], float ox, float py,
                      float len, float w, float swing, u32 col) {
    float cs = cosf(swing), sn = sinf(swing);
    float ay[3] = { F[0] * sn, cs, F[2] * sn };
    float az[3] = { F[0] * cs, -sn, F[2] * cs };
    float piv[3] = { a->x + R[0] * ox, a->y + py, a->z + R[2] * ox };
    float c[3] = { piv[0] - ay[0] * len * .5f, piv[1] - ay[1] * len * .5f, piv[2] - ay[2] * len * .5f };
    draw_box(c, R, ay, az, w * .5f, len * .5f, w * .5f, col);
}
static void draw_actor(const Actor *a) {
    float s = sinf(a->theta), c = cosf(a->theta);
    float F[3] = { s, 0, c }, R[3] = { c, 0, -s }, U[3] = { 0, 1, 0 };
    float sw = sinf(a->phase) * 0.9f * a->moveamt;
    float armR = a->ground ? -sw : -2.7f, armL = a->ground ? sw : -2.7f;
    float tc[3] = { a->x, a->y + 3.0f, a->z };
    draw_box(tc, R, U, F, 1.0f, 1.0f, 0.5f, a->cTorso);
    float hc[3] = { a->x, a->y + 4.55f, a->z };
    draw_box(hc, R, U, F, 0.6f, 0.5f, 0.6f, C_YELLOW);
    // face
    const u32 dark = RGB(30, 30, 30);
    for (int i = 0; i < 5; i++) {
        static const float fo[5][3] = { { -0.2f, 0.12f, 0 }, { 0.2f, 0.12f, 0 }, { 0, -0.22f, 0 }, { -0.27f, -0.14f, 0 }, { 0.27f, -0.14f, 0 } };
        static const float fh[5][2] = { { 0.07f, 0.12f }, { 0.07f, 0.12f }, { 0.18f, 0.04f }, { 0.05f, 0.05f }, { 0.05f, 0.05f } };
        float p[3] = { hc[0] + R[0] * fo[i][0] + F[0] * 0.61f, hc[1] + fo[i][1], hc[2] + R[2] * fo[i][0] + F[2] * 0.61f };
        draw_box(p, R, U, F, fh[i][0], fh[i][1], 0.02f, dark);
    }
    draw_limb(a, R, F, 1.5f, 4.0f, 2.0f, 1.0f, armR, C_YELLOW);
    draw_limb(a, R, F, -1.5f, 4.0f, 2.0f, 1.0f, armL, C_YELLOW);
    draw_limb(a, R, F, 0.5f, 2.0f, 2.0f, 1.0f, sw, a->cLegs);
    draw_limb(a, R, F, -0.5f, 2.0f, 2.0f, 1.0f, -sw, a->cLegs);
}

static void draw_world(void) {
    static const float AX[3] = { 1, 0, 0 }, AY[3] = { 0, 1, 0 }, AZ[3] = { 0, 0, 1 };
    for (int i = 0; i < nblocks; i++) {
        const Block *b = &blocks[i];
        float c[3] = { b->x, b->y, b->z };
        draw_box(c, AX, AY, AZ, b->sx * .5f, b->sy * .5f, b->sz * .5f, b->col);
    }
    for (int i = 0; i < NACT; i++) draw_actor(&act[i]);
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
enum { PG_MAIN, PG_SET, PG_HELP };
static int page = PG_MAIN, sel = 0, paused = 0, quitGame = 0, wantShot = 0, shotN = 0;
static int opt_speed = 1, opt_invert = 0;
static float confirmT = 0, toastT = 0;
static char toastMsg[64];
static void toast(const char *m) { snprintf(toastMsg, sizeof(toastMsg), "%s", m); toastT = 3.0f; }
static const float SENS[3] = { 1.4f, 2.4f, 3.6f };
static const char *SPEEDN[3] = { "Slow", "Normal", "Fast" };

static void do_reset(void) { respawn(&act[0]); act[0].moveamt = 0; toast("Character reset"); }

static void update_player(float dt, float mx, float my, int jump) {
    Actor *p = &act[0];
    p->hurtcd -= dt;
    if (p->hp < 100 && p->hp > 0) p->hp = fminf(100, p->hp + dt);
    float len = sqrtf(mx * mx + my * my);
    if (len > 1) { mx /= len; my /= len; len = 1; }
    float sy = sinf(yaw), cy = cosf(yaw);
    float vx = (cy * mx + sy * my) * WALK_SPEED, vz = (-sy * mx + cy * my) * WALK_SPEED;
    if (len > 0.05f) p->theta += wrapang(atan2f(vx, vz) - p->theta) * fminf(1.0f, dt * 14.0f);
    if (jump && p->ground) { p->vy = JUMP_POWER; p->ground = 0; }
    move_actor(p, vx * dt, vz * dt, dt);
    p->moveamt = len;
    p->phase += dt * len * WALK_SPEED * 0.55f;
    if (p->hp <= 0) respawn(p);
}

// ---------------------------------------------------------------------------
// Top-screen HUD
// ---------------------------------------------------------------------------
static void draw_bubble(int cx, int by, const char *t) {
    int w = text_w(t, 1) + 10, h = 15, x = cx - w / 2, y = by - h;
    d_rect(&ST, x + 1, y, w - 2, h, RGB(255, 255, 255)); d_rect(&ST, x, y + 1, w, h - 2, RGB(255, 255, 255));
    d_rect(&ST, x + 2, y - 1, w - 4, 1, RGB(120, 120, 120)); d_rect(&ST, x + 2, y + h, w - 4, 1, RGB(120, 120, 120));
    d_rect(&ST, x - 1, y + 2, 1, h - 4, RGB(120, 120, 120)); d_rect(&ST, x + w, y + 2, 1, h - 4, RGB(120, 120, 120));
    d_rect(&ST, cx - 3, y + h, 7, 1, RGB(255, 255, 255)); d_rect(&ST, cx - 2, y + h + 1, 5, 1, RGB(255, 255, 255));
    d_rect(&ST, cx - 1, y + h + 2, 3, 1, RGB(255, 255, 255));
    d_text(&ST, x + 5, y + 4, t, 1, RGB(90, 90, 90));
}

static void draw_overlays(void) {
    for (int i = 0; i < NACT; i++) {
        const Actor *a = &act[i];
        float head[3] = { a->x, a->y + 6.2f, a->z };
        int sx, sy; float z;
        if (!proj(head, &sx, &sy, &z) || z > 95.0f) continue;
        int tw = text_w(a->name, 1);
        d_text_sh(&ST, sx - tw / 2, sy - 12, a->name, 1, RGB(255, 255, 255));
        int bw = 34;
        d_rect(&ST, sx - bw / 2 - 1, sy - 1, bw + 2, 5, RGB(20, 30, 20));
        d_rect(&ST, sx - bw / 2, sy, (int)(bw * clampf(a->hp / 100.0f, 0, 1)), 3, RGB(60, 200, 60));
        if (a->sayT > 0 && a->msg[0]) draw_bubble(sx, sy - 14, a->msg);
    }
}

static void draw_hud(void) {
    // player list (top-right)
    for (int i = 0; i < NACT; i++) {
        int y = 5 + i * 12;
        d_rect_a(&ST, 318, y, 78, 11, i == 0 ? RGB(30, 90, 40) : RGB(0, 0, 0), i == 0 ? 190 : 120);
        d_rect(&ST, 321, y + 3, 5, 5, i == 0 ? RGB(240, 200, 40) : RGB(200, 130, 40));
        char nm[12]; snprintf(nm, sizeof(nm), "%.9s", act[i].name);
        d_text(&ST, 329, y + 2, nm, 1, RGB(235, 235, 235));
    }
    // health bar (bottom center)
    float hp = clampf(act[0].hp / 100.0f, 0, 1);
    d_rect_a(&ST, 108, 220, 184, 16, RGB(10, 20, 10), 170);
    d_frame(&ST, 108, 220, 184, 16, RGB(60, 90, 60), 1);
    u32 hc = hp > 0.5f ? RGB(60, 210, 60) : (hp > 0.25f ? RGB(230, 200, 40) : RGB(215, 50, 40));
    d_rect(&ST, 112, 224, (int)(176 * hp), 8, hc);
    d_rect(&ST, 112, 224, (int)(176 * hp), 2, shade(hc, 1.3f));
    d_text_sh(&ST, 292 - 8 - text_w("HEALTH", 1), 225, "HEALTH", 1, RGB(255, 255, 255));
    // Menu button (bottom-left)
    d_rect_a(&ST, 4, 206, 34, 30, RGB(15, 25, 15), 200); d_frame(&ST, 4, 206, 34, 30, RGB(70, 100, 70), 1);
    d_rect(&ST, 15, 212, 12, 6, RGB(240, 240, 240)); d_rect(&ST, 17, 209, 8, 3, RGB(240, 240, 240));
    d_rect(&ST, 19, 214, 4, 4, RGB(20, 30, 20));
    d_text(&ST, 6, 224, "Menu", 1, RGB(255, 255, 255));
    // toast
    if (toastT > 0) {
        int w = text_w(toastMsg, 1) + 16;
        d_rect_a(&ST, TW / 2 - w / 2, 22, w, 16, RGB(0, 0, 0), 190);
        d_text_c(&ST, TW / 2, 26, toastMsg, 1, RGB(255, 255, 255));
    }
    if (paused) {
        d_rect_a(&ST, 0, 0, TW, TH, RGB(0, 0, 0), 120);
        d_text_c(&ST, TW / 2, 100, "Paused", 3, RGB(255, 255, 255));
        d_text_c(&ST, TW / 2, 134, "Tap Resume Game or press START", 1, RGB(210, 210, 210));
    }
}

static void render_top(void) {
    float tgt[3] = { act[0].x, act[0].y + 3.5f, act[0].z };
    cam_update(tgt);
    render_bg();
    draw_world();
    draw_overlays();
    draw_hud();
}

// ---------------------------------------------------------------------------
// Bottom screen: Game Menu (matches the 2013 menu), settings, help, reset prompt
// ---------------------------------------------------------------------------
typedef struct { int x, y, w, h; const char *label, *tag; } Btn;
static const Btn MAINB[7] = {
    { 22, 36, 276, 26, "Resume Game", NULL }, { 22, 66, 276, 26, "Reset Character", NULL },
    { 22, 102, 276, 26, "Game Settings", NULL }, { 22, 147, 90, 26, "Help", NULL },
    { 120, 132, 178, 26, "Screenshot", "PrtSc" }, { 120, 162, 178, 26, "Record Video", "F12" },
    { 22, 198, 276, 26, "Leave Game", NULL } };
static const Btn CONFB[2] = { { 16, 130, 140, 34, "Reset", NULL }, { 164, 130, 140, 34, "Don't Reset", NULL } };
static char setLabel[3][32];

static int inrect(const Btn *b, int px, int py) { return px >= b->x && px < b->x + b->w && py >= b->y && py < b->y + b->h; }

static void draw_btn(const Btn *b, const char *label, int selected) {
    d_rect(&SB, b->x, b->y, b->w, b->h, RGB(8, 8, 8));
    d_frame(&SB, b->x, b->y, b->w, b->h, selected ? RGB(210, 30, 30) : RGB(95, 95, 95), selected ? 2 : 1);
    int tw = b->tag ? text_w_s(b->tag, 1) : 0, lw = text_w_s(label, 1);
    d_text_s(&SB, b->x + (b->w - tw - lw) / 2, b->y + (b->h - 12) / 2, label, 1, RGB(240, 240, 240));
    if (b->tag) d_text_s(&SB, b->x + b->w - tw - 4, b->y + b->h - 12, b->tag, 1, RGB(60, 210, 60));
}

static void render_bottom(void) {
    if (confirmT > 0) {
        d_rect(&SB, 0, 0, BW, BH, RGB(42, 43, 45));
        d_text_c_s(&SB, BW / 2, 56, "Are you sure you want to", 1, RGB(255, 255, 255));
        d_text_c_s(&SB, BW / 2, 78, "reset your character?", 1, RGB(255, 255, 255));
        d_rect(&SB, CONFB[0].x, CONFB[0].y, CONFB[0].w, CONFB[0].h, RGB(47, 182, 255));
        d_rect(&SB, CONFB[1].x, CONFB[1].y, CONFB[1].w, CONFB[1].h, RGB(82, 86, 98));
        d_text_c_s(&SB, CONFB[0].x + CONFB[0].w / 2, CONFB[0].y + 10, "Reset", 1, RGB(255, 255, 255));
        d_text_c_s(&SB, CONFB[1].x + CONFB[1].w / 2, CONFB[1].y + 10, "Don't Reset", 1, RGB(255, 255, 255));
        d_text_c_s(&SB, BW / 2, 190, "Press B again to reset", 1, RGB(150, 150, 150));
        return;
    }
    for (int y = 0; y < BH; y++) d_rect(&SB, 0, y, BW, 1, RGB(26 + y / 24, 26 + y / 24, 28 + y / 24));
    d_rect(&SB, 10, 6, 300, 228, RGB(34, 34, 37));
    d_frame(&SB, 10, 6, 300, 228, RGB(75, 75, 80), 2);
    if (page == PG_MAIN) {
        d_text_c_s(&SB, BW / 2, 14, "Game Menu", 1, RGB(255, 255, 255));
        d_text_c_s(&SB, BW / 2 + 1, 14, "Game Menu", 1, RGB(255, 255, 255));
        for (int i = 0; i < 7; i++) draw_btn(&MAINB[i], MAINB[i].label, sel == i);
    } else if (page == PG_SET) {
        d_text_c_s(&SB, BW / 2, 14, "Game Settings", 1, RGB(255, 255, 255));
        snprintf(setLabel[0], 32, "Camera Speed: %s", SPEEDN[opt_speed]);
        snprintf(setLabel[1], 32, "Invert Camera Y: %s", opt_invert ? "On" : "Off");
        snprintf(setLabel[2], 32, "Studs: %s", opt_studs ? "On" : "Off");
        for (int i = 0; i < 3; i++) { Btn b = { 22, 44 + i * 36, 276, 28, NULL, NULL }; draw_btn(&b, setLabel[i], sel == i); }
        Btn back = { 22, 198, 276, 26, NULL, NULL }; draw_btn(&back, "Back", sel == 3);
    } else {
        d_text_c_s(&SB, BW / 2, 14, "Help", 1, RGB(255, 255, 255));
        static const char *L[] = { "Circle Pad: Move", "C-Stick: Camera", "A: Jump", "B: Reset (press twice)",
                                   "L / R: Zoom camera", "START: Pause" };
        for (int i = 0; i < 6; i++) d_text_s(&SB, 30, 44 + i * 24, L[i], 1, RGB(235, 235, 235));
        Btn back = { 22, 198, 276, 26, NULL, NULL }; draw_btn(&back, "Back", 1);
    }
}

static void menu_touch(int px, int py) {
    if (confirmT > 0) {
        if (inrect(&CONFB[0], px, py)) { do_reset(); confirmT = 0; }
        else if (inrect(&CONFB[1], px, py)) confirmT = 0;
        return;
    }
    if (page == PG_MAIN) {
        for (int i = 0; i < 7; i++) if (inrect(&MAINB[i], px, py)) {
            sel = i;
            switch (i) {
                case 0: paused = 0; break;
                case 1: confirmT = 5.0f; break;
                case 2: page = PG_SET; sel = 0; break;
                case 3: page = PG_HELP; break;
                case 4: wantShot = 1; break;
                case 5: toast("Video recording isn't available on 3DS"); break;
                case 6: quitGame = 1; break;
            }
        }
    } else if (page == PG_SET) {
        for (int i = 0; i < 3; i++) { Btn b = { 22, 44 + i * 36, 276, 28, NULL, NULL };
            if (inrect(&b, px, py)) {
                sel = i;
                if (i == 0) opt_speed = (opt_speed + 1) % 3;
                if (i == 1) opt_invert = !opt_invert;
                if (i == 2) opt_studs = !opt_studs;
            } }
        Btn back = { 22, 198, 276, 26, NULL, NULL };
        if (inrect(&back, px, py)) { page = PG_MAIN; sel = 2; }
    } else {
        Btn back = { 22, 198, 276, 26, NULL, NULL };
        if (inrect(&back, px, py)) { page = PG_MAIN; sel = 3; }
    }
}

// ---------------------------------------------------------------------------
// Screenshot (BMP to sdmc:/roblox2013/)
// ---------------------------------------------------------------------------
static void save_screenshot(void) {
    mkdir("sdmc:/roblox2013", 0777);
    char path[64]; FILE *f = NULL;
    for (; shotN < 1000; shotN++) {
        snprintf(path, sizeof(path), "sdmc:/roblox2013/shot_%03d.bmp", shotN);
        f = fopen(path, "rb");
        if (!f) break;
        fclose(f);
    }
    f = fopen(path, "wb");
    if (!f) { toast("Couldn't save screenshot"); return; }
    u8 hdr[54]; memset(hdr, 0, sizeof(hdr));
    u32 size = 54 + TW * TH * 3, off = 54, dib = 40; s32 w = TW, h = TH; u16 pl = 1, bpp = 24;
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(hdr + 2, &size, 4); memcpy(hdr + 10, &off, 4); memcpy(hdr + 14, &dib, 4);
    memcpy(hdr + 18, &w, 4); memcpy(hdr + 22, &h, 4); memcpy(hdr + 26, &pl, 2); memcpy(hdr + 28, &bpp, 2);
    fwrite(hdr, 1, 54, f);
    static u8 rowb[TW * 3];
    for (int y = TH - 1; y >= 0; y--) {
        for (int x = 0; x < TW; x++) { u32 c = topbuf[y * TW + x]; rowb[x * 3] = c; rowb[x * 3 + 1] = c >> 8; rowb[x * 3 + 2] = c >> 16; }
        fwrite(rowb, 1, sizeof(rowb), f);
    }
    fclose(f);
    char m[64]; snprintf(m, sizeof(m), "Screenshot saved: shot_%03d.bmp", shotN++);
    toast(m);
}

// ---------------------------------------------------------------------------
// Presenting to the 3DS framebuffers (BGR8, rotated 90 degrees)
// ---------------------------------------------------------------------------
static void blit(u8 *fb, const u32 *buf, int w, int h) {
    for (int x = 0; x < w; x++) {
        u8 *col = fb + x * h * 3; const u32 *src = buf + x;
        for (int y = 0; y < h; y++) {
            u32 c = src[y * w]; u8 *o = col + (h - 1 - y) * 3;
            o[0] = (u8)c; o[1] = (u8)(c >> 8); o[2] = (u8)(c >> 16);
        }
    }
}

static float stick(int v, float range, float dead) {
    float f = v / range;
    if (fabsf(f) < dead) return 0;
    return clampf(f, -1.0f, 1.0f);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    osSetSpeedupEnable(true);   // New 3DS 804 MHz mode
    gfxInitDefault();
    gfxSet3D(false);
    srand((unsigned)osGetTime());
    gen_clouds(); build_world(); init_actors();
    
    u64 last = osGetTime();

    while (aptMainLoop() && !quitGame) {
        u64 now = osGetTime();
        float dt = (float)(now - last) / 1000.0f; last = now;
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.001f) dt = 0.001f;

        hidScanInput();
        u32 kd = hidKeysDown(), kh = hidKeysHeld();
        circlePosition cp, cs; touchPosition tp;
        hidCircleRead(&cp); hidCstickRead(&cs);

        if (kd & KEY_TOUCH) { hidTouchRead(&tp); menu_touch(tp.px, tp.py); }
        // D-pad control for bottom menu
        if (kd & KEY_DUP) {
            if (page == PG_MAIN) sel = (sel + 6) % 7;
            else if (page == PG_SET) sel = (sel + 3) % 4;
        }
        if (kd & KEY_DDOWN) {
            if (page == PG_MAIN) sel = (sel + 1) % 7;
            else if (page == PG_SET) sel = (sel + 1) % 4;
        }
        if (kd & KEY_A) {
            if (page == PG_MAIN) {
                switch (sel) {
                    case 0: paused = 0; break;
                    case 1: confirmT = 5.0f; break;
                    case 2: page = PG_SET; sel = 0; break;
                    case 3: page = PG_HELP; break;
                    case 4: wantShot = 1; break;
                    case 5: toast("Video recording isn't available on 3DS"); break;
                    case 6: quitGame = 1; break;
                }
            } else if (page == PG_SET) {
                if (sel == 0) opt_speed = (opt_speed + 1) % 3;
                else if (sel == 1) opt_invert = !opt_invert;
                else if (sel == 2) opt_studs = !opt_studs;
                else if (sel == 3) { page = PG_MAIN; sel = 2; }
            } else if (page == PG_HELP) {
                page = PG_MAIN; sel = 3;
            }
        }

        if (kd & KEY_START) paused = !paused;
        if (kd & KEY_B) { if (confirmT > 0) { do_reset(); confirmT = 0; } else confirmT = 5.0f; }
        if (confirmT > 0) confirmT -= dt;
        if (toastT > 0) toastT -= dt;
        

        if (!paused) {
            float sx = stick(cs.dx, 140.0f, 0.15f), sy = stick(cs.dy, 140.0f, 0.15f);
            yaw += sx * SENS[opt_speed] * dt;
            pitch += (opt_invert ? -sy : sy) * SENS[opt_speed] * 0.8f * dt;
            pitch = clampf(pitch, -1.35f, 0.3f);
            if (kh & KEY_L) camdist -= 14.0f * dt;
            if (kh & KEY_R) camdist += 14.0f * dt;
            camdist = clampf(camdist, 4.0f, 40.0f);
            update_player(dt, stick(cp.dx, 150.0f, 0.12f), stick(cp.dy, 150.0f, 0.12f), (kh & KEY_A) != 0);
            
        }

        render_top();
        if (wantShot) { save_screenshot(); wantShot = 0; }
        render_bottom();
        blit(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), topbuf, TW, TH);
        blit(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL), botbuf, BW, BH);
        gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
