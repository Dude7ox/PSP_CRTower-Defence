#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspge.h>
#include <psputils.h>
#include <math.h>
#include <string.h>

#include "render.h"
#include "font_data.h"

#define BUF_W 512
#define LIST_WORDS 131072
#define MAX_VERTS 262144
#define MAX_BATCHES 2048
#define MAX_BATCH_VERTS 30000

#define VERT_FMT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

typedef struct {
    float u, v;
    uint32_t color;
    float x, y, z;
} Vertex;

typedef struct {
    int tex;        /* -1 = solid */
    int start;
    int count;
} Batch;

static unsigned int __attribute__((aligned(16))) dlist[LIST_WORDS];
static Vertex vbuf[MAX_VERTS] __attribute__((aligned(64)));
static Batch batches[MAX_BATCHES];
static int vn, bn, cur_tex;
static uint32_t vram_base;

static uint32_t font_tex_vram;

/* 256-step sin/cos table for the many circle/arc vertices. Keeps libm out
   of the per-frame path drawing hot spots. */
#define TRIG_N 256
static float sin_tab[TRIG_N];
static float cos_tab[TRIG_N];

static void init_trig(void) {
    for (int i = 0; i < TRIG_N; i++) {
        float a = (float)i * (6.2831853f / (float)TRIG_N);
        sin_tab[i] = sinf(a);
        cos_tab[i] = cosf(a);
    }
}

static inline void sincos_fast(float a, float *s, float *c) {
    int idx = (int)(a * ((float)TRIG_N / 6.2831853f)) & (TRIG_N - 1);
    *s = sin_tab[idx];
    *c = cos_tab[idx];
}

static uint32_t font_linear[FONT_ATLAS_W * FONT_ATLAS_H];
static uint32_t font_swizzled[FONT_ATLAS_W * FONT_ATLAS_H] __attribute__((aligned(16)));

/* ------------------------------------------------------------------ */
/* font atlas                                                         */
/* ------------------------------------------------------------------ */

static void swizzle_8888(uint32_t *out, const uint32_t *in, int w, int h) {
    int rowblocks = (w * 4) / 16;
    const unsigned char *src = (const unsigned char *)in;
    unsigned char *dst = (unsigned char *)out;
    for (int y = 0; y < h; y++) {
        int by = y / 8, ry = y % 8;
        for (int bx = 0; bx < rowblocks; bx++) {
            memcpy(dst + (by * rowblocks + bx) * 128 + ry * 16,
                   src + y * w * 4 + bx * 16, 16);
        }
    }
}

static int glyph_index(uint32_t cp) {
    if (cp >= 1 && cp <= 4) return 63 + (int)cp; /* face-button icons */
    if (cp >= 'a' && cp <= 'z') cp -= 32;
    if (cp >= 32 && cp <= 95) return (int)cp - 32;
    return -1;
}

static uint32_t utf8_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    uint32_t c = *s++;
    if (c < 0x80) { *p = (const char *)s; return c; }
    if ((c & 0xE0) == 0xC0) {
        c = (uint32_t)(c & 0x1F) << 6;
        if ((*s & 0xC0) == 0x80) c |= (uint32_t)(*s++ & 0x3F);
        *p = (const char *)s; return c;
    }
    if ((c & 0xF0) == 0xE0) {
        c = (uint32_t)(c & 0x0F) << 12;
        if ((*s & 0xC0) == 0x80) c |= (uint32_t)(*s++ & 0x3F) << 6;
        if ((*s & 0xC0) == 0x80) c |= (uint32_t)(*s++ & 0x3F);
        *p = (const char *)s; return c;
    }
    if ((c & 0xF8) == 0xF0) {
        c = (uint32_t)(c & 0x07) << 18;
        if ((*s & 0xC0) == 0x80) c |= (uint32_t)(*s++ & 0x3F) << 12;
        if ((*s & 0xC0) == 0x80) c |= (uint32_t)(*s++ & 0x3F) << 6;
        if ((*s & 0xC0) == 0x80) c |= (uint32_t)(*s++ & 0x3F);
        *p = (const char *)s; return c;
    }
    *p = (const char *)s;
    return '?';
}

/* ------------------------------------------------------------------ */
/* batching                                                            */
/* ------------------------------------------------------------------ */

static void begin_batch(int tex) {
    if (bn > 0) batches[bn - 1].count = vn - batches[bn - 1].start;
    if (bn >= MAX_BATCHES) return;
    batches[bn].tex = tex;
    batches[bn].start = vn;
    batches[bn].count = 0;
    bn++;
    cur_tex = tex;
}

static void put(float x, float y, float u, float v, uint32_t color, int tex) {
    if (tex != cur_tex) begin_batch(tex);
    if (vn >= MAX_VERTS) return;
    vbuf[vn].u = u;
    vbuf[vn].v = v;
    vbuf[vn].color = color;
    vbuf[vn].x = x;
    vbuf[vn].y = y;
    vbuf[vn].z = 0.0f;
    vn++;
}

static void solid_vert(float x, float y, uint32_t c) { put(x, y, 0, 0, c, -1); }

static void tri(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t c) {
    solid_vert(x0, y0, c); solid_vert(x1, y1, c); solid_vert(x2, y2, c);
}

static void quad(float x0, float y0, float x1, float y1,
                 float x2, float y2, float x3, float y3, uint32_t c) {
    tri(x0, y0, x1, y1, x2, y2, c);
    tri(x0, y0, x2, y2, x3, y3, c);
}

static void flush_chunk(void);

/* Called at the start of a primitive. A primitive is never split across a
   chunk boundary, so reserve enough vertices for the whole primitive. */
static void reserve(int n) {
    if (vn + n > MAX_VERTS - 16 || bn + 2 > MAX_BATCHES) {
        flush_chunk();
    } else if (bn > 0 && (vn - batches[bn - 1].start) + n > MAX_BATCH_VERTS) {
        /* The GE primitive command has a 16-bit vertex count. Keep every
           batch well below 65535 vertices or large frames are truncated. */
        int tex = cur_tex;
        begin_batch(tex);
    }
}

void r_rect(float x, float y, float w, float h, uint32_t c) {
    reserve(6);
    quad(x, y, x + w, y, x + w, y + h, x, y + h, c);
}

void r_outline(float x, float y, float w, float h, float t, uint32_t c) {
    r_rect(x, y, w, t, c);
    r_rect(x, y + h - t, w, t, c);
    r_rect(x, y, t, h, c);
    r_rect(x + w - t, y, t, h, c);
}

void r_line(float x0, float y0, float x1, float y1, float t, uint32_t c) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * (t * 0.5f);
    float ny = dx / len * (t * 0.5f);
    reserve(6);
    quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny,
         x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
}

void r_circle(float cx, float cy, float r, uint32_t c) {
    if (r <= 0.1f) return;
    int n = r > 20 ? 32 : (r > 8 ? 24 : 16);
    reserve(n * 3);
    float step = 6.2831853f / (float)n;
    float c0 = 1.0f, s0 = 0.0f;
    for (int i = 0; i < n; i++) {
        float c1, s1;
        sincos_fast((float)(i + 1) * step, &s1, &c1);
        tri(cx, cy,
            cx + c0 * r, cy + s0 * r,
            cx + c1 * r, cy + s1 * r, c);
        c0 = c1; s0 = s1;
    }
}

void r_ring(float cx, float cy, float r, float t, uint32_t c) {
    int n = r > 20 ? 40 : 28;
    float ri = r - t * 0.5f, ro = r + t * 0.5f;
    if (ri < 0.1f) ri = 0.1f;
    reserve(n * 6);
    float step = 6.2831853f / (float)n;
    float c0 = 1.0f, s0 = 0.0f;
    for (int i = 0; i < n; i++) {
        float c1, s1;
        sincos_fast((float)(i + 1) * step, &s1, &c1);
        quad(cx + c0 * ri, cy + s0 * ri, cx + c0 * ro, cy + s0 * ro,
             cx + c1 * ro, cy + s1 * ro, cx + c1 * ri, cy + s1 * ri, c);
        c0 = c1; s0 = s1;
    }
}

void r_arc(float cx, float cy, float r, float a0, float a1, float t, uint32_t c) {
    float span = a1 - a0;
    int n = (int)(fabsf(span) * r / 5.0f);
    if (n < 2) n = 2;
    if (n > 64) n = 64;
    float ri = r - t * 0.5f, ro = r + t * 0.5f;
    reserve(n * 6);
    for (int i = 0; i < n; i++) {
        float u0 = a0 + span * (float)i / n;
        float u1 = a0 + span * (float)(i + 1) / n;
        float c0, s0, c1, s1;
        sincos_fast(u0, &s0, &c0);
        sincos_fast(u1, &s1, &c1);
        quad(cx + c0 * ri, cy + s0 * ri, cx + c0 * ro, cy + s0 * ro,
             cx + c1 * ro, cy + s1 * ro, cx + c1 * ri, cy + s1 * ri, c);
    }
}

void r_dashed_circle_phase(float cx, float cy, float r, float t, uint32_t c,
                           float phase) {
    int n = 40;
    float step = 6.2831853f / (float)n;
    for (int i = 0; i < n; i += 2) {
        float a0 = phase + (float)i * step;
        float a1 = phase + (float)(i + 1) * step - 0.03f;
        r_arc(cx, cy, r, a0, a1, t, c);
    }
}

void r_dashed_circle(float cx, float cy, float r, float t, uint32_t c) {
    r_dashed_circle_phase(cx, cy, r, t, c, 0.0f);
}

void r_rot_rect(float cx, float cy, float angle,
                float lx0, float ly0, float lx1, float ly1, uint32_t c) {
    reserve(6);
    float ca = cosf(angle), sa = sinf(angle);
    float x0 = cx + lx0 * ca - ly0 * sa;
    float y0 = cy + lx0 * sa + ly0 * ca;
    float x1 = cx + lx1 * ca - ly0 * sa;
    float y1 = cy + lx1 * sa + ly0 * ca;
    float x2 = cx + lx1 * ca - ly1 * sa;
    float y2 = cy + lx1 * sa + ly1 * ca;
    float x3 = cx + lx0 * ca - ly1 * sa;
    float y3 = cy + lx0 * sa + ly1 * ca;
    quad(x0, y0, x1, y1, x2, y2, x3, y3, c);
}

void r_poly(const float *xy, int n, uint32_t c) {
    if (n < 3) return;
    reserve((n - 1) * 3);
    for (int i = 1; i < n - 1; i++)
        tri(xy[0], xy[1], xy[i * 2], xy[i * 2 + 1],
            xy[(i + 1) * 2], xy[(i + 1) * 2 + 1], c);
}

void r_star(float cx, float cy, float r1, float r2, int points, float rot, uint32_t c) {
    if (points < 3) return;
    reserve(points * 6);
    for (int i = 0; i < points * 2; i++) {
        float a0 = rot + (float)i / (points * 2) * 6.2831853f;
        float a1 = rot + (float)(i + 1) / (points * 2) * 6.2831853f;
        float r0 = (i & 1) ? r2 : r1;
        float rn = (i & 1) ? r1 : r2;
        tri(cx, cy,
            cx + cosf(a0) * r0, cy + sinf(a0) * r0,
            cx + cosf(a1) * rn, cy + sinf(a1) * rn, c);
    }
}

/* ------------------------------------------------------------------ */
/* text                                                                */
/* ------------------------------------------------------------------ */

float r_text_width(const char *s, int scale) {
    float w = 0.0f;
    const char *p = s;
    while (*p) {
        unsigned char ch = (unsigned char)*p;
        uint32_t cp = ch;
        if (ch < 0x80) p++;
        else cp = utf8_next(&p);
        if (cp == ' ') { w += 6.0f * scale; continue; }
        int gi = glyph_index(cp);
        if (gi < 0) gi = glyph_index('?');
        w += (float)font_adv[gi] * scale;
    }
    return w;
}

void r_text(float x, float y, const char *s, uint32_t color, int scale) {
    float cx = x;
    float gw = (float)FONT_CELL_W * scale;
    float gh = (float)FONT_CELL_H * scale;
    const char *p = s;
    while (*p) {
        unsigned char ch = (unsigned char)*p;
        uint32_t cp = ch;
        if (ch < 0x80) p++;
        else cp = utf8_next(&p);
        if (cp == ' ') { cx += 6.0f * scale; continue; }
        int gi = glyph_index(cp);
        if (gi < 0) gi = glyph_index('?');
        reserve(6);
        int gx = (gi % FONT_COLS) * FONT_CELL_W;
        int gy = (gi / FONT_COLS) * FONT_CELL_H;
        float u0 = (float)gx, v0 = (float)gy;
        float u1 = u0 + FONT_CELL_W, v1 = v0 + FONT_CELL_H;
        float x0 = cx, y0 = y, x1 = cx + gw, y1 = y + gh;
        put(x0, y0, u0, v0, color, 0);
        put(x1, y0, u1, v0, color, 0);
        put(x1, y1, u1, v1, color, 0);
        put(x0, y0, u0, v0, color, 0);
        put(x1, y1, u1, v1, color, 0);
        put(x0, y1, u0, v1, color, 0);
        cx += (float)font_adv[gi] * scale;
    }
}

void r_text_center(float cx, float y, const char *s, uint32_t color, int scale) {
    r_text(cx - r_text_width(s, scale) * 0.5f, y, s, color, scale);
}

void r_text_right(float rx, float y, const char *s, uint32_t color, int scale) {
    r_text(rx - r_text_width(s, scale), y, s, color, scale);
}

void r_text_glow(float x, float y, const char *s, uint32_t color,
                 uint32_t glow, int scale) {
    /* Crisp 1-pixel shadow, never a filtered/blurred halo. */
    r_text(x + scale, y + scale, s, C_ALPHA(glow, 200), scale);
    r_text(x, y, s, color, scale);
}

/* Greedy word wrap with the proportional advance. Returns the next y. */
float r_text_wrap(float x, float y, float maxw, float lineh,
                  const char *utf8, uint32_t color, int scale) {
    float spacew = 6.0f * scale;
    char line[512];
    int li = 0;
    float lw = 0.0f;
    float cy = y;

#define FLUSH_LINE() do { \
        if (li > 0) { line[li] = 0; r_text(x, cy, line, color, scale); \
                      cy += lineh; li = 0; lw = 0.0f; } \
    } while (0)

    const char *p = utf8;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        const char *w = p;
        float ww = 0.0f;
        while (*p && *p != ' ') {
            const char *before = p;
            uint32_t cp = utf8_next(&p);
            (void)before;
            int gi = glyph_index(cp);
            if (gi < 0) gi = glyph_index('?');
            ww += (float)font_adv[gi] * scale;
        }
        const char *we = p;

        if (lw > 0.0f && lw + spacew + ww > maxw) FLUSH_LINE();
        if (lw > 0.0f) {
            if (li < (int)sizeof(line) - 1) line[li++] = ' ';
            lw += spacew;
        }

        const char *q = w;
        while (q < we) {
            const char *b = q;
            uint32_t cp = utf8_next(&q);
            int gi = glyph_index(cp);
            if (gi < 0) gi = glyph_index('?');
            float aw = (float)font_adv[gi] * scale;
            if (lw > 0.0f && lw + aw > maxw) FLUSH_LINE();
            while (b < q && li < (int)sizeof(line) - 1) line[li++] = *b++;
            lw += aw;
        }
    }
    FLUSH_LINE();
#undef FLUSH_LINE
    return cy;
}

/* ------------------------------------------------------------------ */
/* init / frame                                                        */
/* ------------------------------------------------------------------ */

void render_init(void) {
    init_trig();
    vram_base = (uint32_t)sceGeEdramGetAddr();
    void *fbp0 = (void *)0;
    void *fbp1 = (void *)(BUF_W * SCR_H * 2);
    font_tex_vram = BUF_W * SCR_H * 2 * 2;
    void *zbp = (void *)(font_tex_vram + FONT_ATLAS_W * FONT_ATLAS_H * 4);

    for (int i = 0; i < FONT_ATLAS_W * FONT_ATLAS_H; i++) {
        uint32_t a = font_coverage[i];
        font_linear[i] = C_RGBA(255, 255, 255, (int)a);
    }
    swizzle_8888(font_swizzled, font_linear, FONT_ATLAS_W, FONT_ATLAS_H);
    memcpy((void *)(vram_base + font_tex_vram), font_swizzled, sizeof(font_swizzled));
    sceKernelDcacheWritebackAll();

    sceGuInit();
    sceGuStart(GU_DIRECT, dlist);
    sceGuDrawBuffer(GU_PSM_5650, fbp0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, fbp1, BUF_W);
    sceGuDepthBuffer(zbp, BUF_W);
    sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_DITHER);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void render_shutdown(void) {
    sceGuDisplay(GU_FALSE);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceGuTerm();
}

static void set_2d_state(void) {
    sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
}

static void reset_batches(void) {
    vn = 0;
    bn = 0;
    cur_tex = -1;
    begin_batch(-1);
}

static void draw_batches(void) {
    if (bn > 0) batches[bn - 1].count = vn - batches[bn - 1].start;

    /* The GE reads this array through its own bus path: dirty CPU cache lines
       must reach RAM before the display list is kicked, or hardware draws
       stale/garbled vertices while the emulator looks fine. */
    if (vn > 0)
        sceKernelDcacheWritebackRange(vbuf, (unsigned int)(vn * sizeof(Vertex)));

    int last_tex = -2;
    for (int i = 0; i < bn; i++) {
        if (batches[i].count <= 0) continue;
        if (batches[i].tex < 0) {
            if (last_tex != -1) {
                sceGuDisable(GU_TEXTURE_2D);
                last_tex = -1;
            }
        } else {
            if (last_tex != 0) {
                sceGuEnable(GU_TEXTURE_2D);
                sceGuTexMode(GU_PSM_8888, 0, 0, GU_TRUE);
                sceGuTexImage(0, FONT_ATLAS_W, FONT_ATLAS_H, FONT_ATLAS_W,
                              (void *)(vram_base + font_tex_vram));
                last_tex = 0;
            }
        }
        sceGumDrawArray(GU_TRIANGLES, VERT_FMT, batches[i].count, 0,
                        &vbuf[batches[i].start]);
    }
    sceGuDisable(GU_TEXTURE_2D);
}

/* Flush a full chunk. The finish/sync is required before the vertex buffer
   can be reused: the GE reads vertices asynchronously from RAM. */
static void flush_chunk(void) {
    if (vn == 0) {
        reset_batches();
        return;
    }
    draw_batches();
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    reset_batches();
    sceGuStart(GU_DIRECT, dlist);
    set_2d_state();
}

void render_frame_begin(void) {
    sceGuStart(GU_DIRECT, dlist);
    set_2d_state();
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    reset_batches();
}

void render_frame_end(void) {
    draw_batches();
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}
