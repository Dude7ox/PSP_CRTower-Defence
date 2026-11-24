#ifndef CRTD_RENDER_H
#define CRTD_RENDER_H

#include <stdint.h>

#define SCR_W 480
#define SCR_H 272

/* Packed PSP vertex colour: 0xAABBGGRR. */
#define C_RGB(r, g, b) ((uint32_t)((r) & 255) | ((uint32_t)((g) & 255) << 8) | \
                        ((uint32_t)((b) & 255) << 16) | 0xFF000000u)
#define C_RGBA(r, g, b, a) ((uint32_t)((r) & 255) | ((uint32_t)((g) & 255) << 8) | \
                            ((uint32_t)((b) & 255) << 16) | ((uint32_t)((a) & 255) << 24))
#define C_ALPHA(c, a) (((c) & 0x00FFFFFFu) | ((uint32_t)((a) & 255) << 24))

void render_init(void);
void render_shutdown(void);
void render_frame_begin(void);
void render_frame_end(void);

/* Solid primitives. Every call keeps painter's order with text. */
void r_rect(float x, float y, float w, float h, uint32_t color);
void r_outline(float x, float y, float w, float h, float t, uint32_t color);
void r_line(float x0, float y0, float x1, float y1, float t, uint32_t color);
void r_circle(float cx, float cy, float r, uint32_t color);
void r_ring(float cx, float cy, float r, float t, uint32_t color);
void r_arc(float cx, float cy, float r, float a0, float a1, float t, uint32_t color);
void r_dashed_circle(float cx, float cy, float r, float t, uint32_t color);
void r_dashed_circle_phase(float cx, float cy, float r, float t, uint32_t color,
                           float phase);
void r_rot_rect(float cx, float cy, float angle,
                float lx0, float ly0, float lx1, float ly1, uint32_t color);
void r_poly(const float *xy, int n, uint32_t color);
void r_star(float cx, float cy, float r1, float r2, int points, float rot, uint32_t color);

/* Text. Coordinates are top-left; 8x10 glyphs, 7 px advance at scale 1. */
void r_text(float x, float y, const char *utf8, uint32_t color, int scale);
void r_text_center(float cx, float y, const char *utf8, uint32_t color, int scale);
void r_text_right(float rx, float y, const char *utf8, uint32_t color, int scale);
float r_text_width(const char *utf8, int scale);
float r_text_wrap(float x, float y, float maxw, float lineh,
                  const char *utf8, uint32_t color, int scale);
void r_text_glow(float x, float y, const char *utf8, uint32_t color,
                 uint32_t glow, int scale);

#endif
