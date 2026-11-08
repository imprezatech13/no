// ROBLOX 2013 (fan tribute) for the New Nintendo 3DS
// Software-rendered 3D on the top screen, 2013-style Game Menu on the bottom.

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

static inline u32 mixc(u32 a, u32 b, int t) {
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

// 5x7 Font for Top Screen
typedef struct { char c; u8 r[8]; } Glyph;
static const Glyph FONT[] = {
    { ' ', { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 'A', { 14, 17, 17, 31, 17, 17, 17, 0 } }, { 'B', { 30, 17, 17, 30, 17, 17, 30, 0 } },
    { 'C', { 14, 17, 16, 16, 16, 17, 14, 0 } }, { 'D', { 30, 17, 17, 17, 17, 17, 30, 0 } },
    { 'E', { 31, 16, 16, 30, 16, 16, 31, 0 } }, { 'F', { 31, 16, 16, 30, 16, 16, 16, 0 } },
    { 'G', { 14, 17, 16, 23, 17, 17, 15, 0 } }, { 'H', { 17, 17, 17, 31, 17, 17, 17, 0 } },
    { 'I', { 14, 4, 4, 4, 4, 4, 14, 0 } }, { 'J', { 7, 2, 2, 2, 2, 18, 12, 0 } },
    { 'K', { 17, 18, 20, 24, 20, 18, 17, 0 } }, { 'L', { 16, 16, 16, 16, 16, 16, 31, 0 } },
    { 'M', { 17, 27, 21, 21, 17, 17, 17, 0 } }, { 'N', { 17, 25, 21, 19, 17, 17, 17, 0 } },
    { 'O', { 14, 17, 17, 17, 17, 17, 14, 0 } }, { 'P', { 30, 17, 17, 30, 16, 16, 16, 0 } },
    { 'Q', { 14, 17, 17, 17, 21, 18, 13, 0 } }, { 'R', { 30, 17, 17, 30, 20, 18, 17, 0 } },
    { 'S', { 15, 16, 16, 14, 1, 1, 30, 0 } }, { 'T', { 31, 4, 4, 4, 4, 4, 4, 0 } },
    { 'U', { 17, 17, 17, 17, 17, 17, 14, 0 } }, { 'V', { 17, 17, 17, 17, 17, 10, 4, 0 } },
    { 'W', { 17, 17, 17, 21, 21, 21, 10, 0 } }, { 'X', { 17, 17, 10, 4, 10, 17, 17, 0 } },
    { 'Y', { 17, 17, 10, 4, 4, 4, 4, 0 } }, { 'Z', { 31, 1, 2, 4, 8, 16, 31, 0 } },
    { '0', { 14, 17, 19, 21, 25, 17, 14, 0 } }, { '1', { 4, 12, 4, 4, 4, 4, 14, 0 } },
    { '2', { 14, 17, 1, 2, 4, 8, 31, 0 } }, { '3', { 31, 2, 4, 2, 1, 17, 14, 0 } },
    { '4', { 2, 6, 10, 18, 31, 2, 2, 0 } }, { '5', { 31, 16, 30, 1, 1, 17, 14, 0 } },
    { '6', { 6, 8, 16, 30, 17, 17, 14, 0 } }, { '7', { 31, 1, 2, 4, 8, 8, 8, 0 } },
    { '8', { 14, 17, 17, 14, 17, 17, 14, 0 } }, { '9', { 14, 17, 17, 15, 1, 2, 12, 0 } },
    { 'H', { 17, 17, 17, 31, 17, 17, 17, 0 } }, { 'E', { 31, 16, 16, 30, 16, 16, 31, 0 } },
    { 'A', { 14, 17, 17, 31, 17, 17, 17, 0 } }, { 'L', { 16, 16, 16, 16, 16, 16, 31, 0 } },
    { 'T', { 31, 4, 4, 4, 4, 4, 4, 0 } }, { 'a', { 0, 0, 14, 1, 15, 17, 15, 0 } },
    { 'b', { 16, 16, 22, 25, 17, 17, 30, 0 } }, { 'c', { 0, 0, 14, 16, 16, 17, 14, 0 } },
    { 'd', { 1, 1, 13, 19, 17, 17, 15, 0 } }, { 'e', { 0, 0, 14, 17, 31, 16, 14, 0 } },
    { 'i', { 4, 0, 12, 4, 4, 4, 14, 0 } }, { 'n', { 0, 0, 22, 25, 17, 17, 17, 0 } },
    { 'g', { 0, 0, 15, 17, 17, 15, 1, 14 } }, { 's', { 0, 0, 15, 16, 14, 1, 30, 0 } },
    { 'p', { 0, 0, 30, 17, 17, 30, 16, 16 } }, { 'o', { 0, 0, 14, 17, 17, 17, 14, 0 } },
    { 'r', { 0, 0, 22, 25, 16, 16, 16, 0 } }, { 't', { 8, 8, 28, 8, 8, 9, 6, 0 } },
    { '-', { 0, 0, 0, 31, 0, 0, 0, 0 } }
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

// Simple Font for Bottom Screen Menu
typedef struct { char c; u8 r[10]; } GlyphSimple;
static const GlyphSimple FONT_SIMPLE[] = {
    { ' ', { 0,0,0,0,0,0,0,0,0,0 } }, { 'A', { 30,51,51,51,63,51,51,51,51,0 } },
    { 'B', { 62,51,51,62,51,51,51,62,0,0 } }, { 'C', { 30,51,48,48,48,48,51,30,0,0 } },
    { 'D', { 60,54,51,51,51,51,54,60,0,0 } }, { 'E', { 63,48,48,60,48,48,48,63,0,0 } },
    { 'F', { 63,48,48,60,48,48,48,48,0,0 } }, { 'G', { 30,51,48,48,55,51,51,30,0,0 } },
    { 'H', { 51,51,51,63,51,51,51,51,0,0 } }, { 'I', { 30,12,12,12,12,12,12,30,0,0 } },
    { 'J', { 15,6,6,6,6,38,38,28,0,0 } }, { 'K', { 51,54,60,56,60,54,51,51,0,0 } },
    { 'L', { 48,48,48,48,48,48,48,63,0,0 } }, { 'M', { 51,63,63,51,51,51,51,51,0,0 } },
    { 'N', { 51,51,59,63,55,51,51,51,0,0 } }, { 'O', { 30,51,51,51,51,51,51,30,0,0 } },
    { 'P', { 62,51,51,62,48,48,48,48,0,0 } }, { 'Q', { 30,51,51,51,51,55,30,3,0,0 } },
    { 'R', { 62,51,51,62,60,54,51,51,0,0 } }, { 'S', { 31,48,48,30,3,3,51,30,0,0 } },
    { 'T', { 63,12,12,12,12,12,12,12,0,0 } }, { 'U', { 51,51,51,51,51,51,51,30,0,0 } },
    { 'V', { 51,51,51,51,51,30,12,12,0,0 } }, { 'W', { 51,51,51,51,63,63,51,51,0,0 } },
    { 'X', { 51,51,30,12,30,51,51,51,0,0 } }, { 'Y', { 51,51,30,12,12,12,12,12,0,0 } },
    { 'Z', { 63,3,6,12,24,48,48,63,0,0 } }, { '0', { 30,51,51,51,51,51,51,30,0,0 } },
    { '1', { 12,28,12,12,12,12,12,30,0,0 } }, { '2', { 30,51,3,6,12,24,48,63,0,0 } },
    { '3', { 30,51,3,14,3,3,51,30,0,0 } }, { '4', { 6,14,22,38,63,6,6,6,0,0 } },
    { '5', { 63,48,62,3,3,3,51,30,0,0 } }, { '6', { 14,24,48,62,51,51,51,30,0,0 } },
    { '7', { 63,3,6,12,24,24,24,24,0,0 } }, { '8', { 30,51,51,30,51,51,51,30,0,0 } },
    { '9', { 30,51,51,31,3,6,12,24,0,0 } }, { 'a', { 0,0,30,3,31,51,51,31,0,0 } },
    { 'b', { 48,48,62,51,51,51,51,62,0,0 } }, { 'c', { 0,0,30,48,48,48,51,30,0,0 } },
    { 'd', { 3,3,31,51,51,51,51,31,0,0 } }, { 'e', { 0,0,30,51,63,48,51,30,0,0 } },
    { 'f', { 14,24,60,24,24,24,24,24,0,0 } }, { 'g', { 0,0,31,51,51,31,3,30,0,0 } },
    { 'h', { 48,48,62,51,51,51,51,51,0,0 } }, { 'i', { 12,0,28,12,12,12,12,30,0,0 } },
    { 'j', { 6,0,14,6,6,6,38,28,0,0 } }, { 'k', { 48,48,51,54,60,54,51,51,0,0 } },
    { 'l', { 28,12,12,12,12,12,12,30,0,0 } }, { 'm', { 0,0,54,63,51,51,51,51,0,0 } },
    { 'n', { 0,0,62,51,51,51,51,51,0,0 } }, { 'o', { 0,0,30,51,51,51,51,30,0,0 } },
    { 'p', { 0,0,62,51,51,62,48,48,0,0 } }, { 'q', { 0,0,31,51,51,31,3,3,0,0 } },
    { 'r', { 0,0,62,51,48,48,48,48,0,0 } }, { 's', { 0,0,31,48,30,3,51,30,0,0 } },
    { 't', { 24,24,60,24,24,24,24,14,0,0 } }, { 'u', { 0,0,51,51,51,51,51,31,0,0 } },
    { 'v', { 0,0,51,51,51,30,12,12,0,0 } }, { 'w', { 0,0,51,51,51,63,63,51,0,0 } },
    { 'x', { 0,0,51,30,12,30,51,51,0,0 } }, { 'y', { 0,0,51,51,51,31,3,30,0,0 } },
    { 'z', { 0,0,63,6,12,24,48,63,0,0 } }, { ':', { 0,12,12,0,0,12,12,0,0,0 } },
    { 'PrtSc', {0} }, { 'F12', {0} }, { ' ', {0} }
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
// Camera + 3D rasteriser
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
// Sky + Baseplate
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
                if (X < -150 || X > 150 || Z < -150 || Z > 150) { row[x] = C_HORIZON; continue
