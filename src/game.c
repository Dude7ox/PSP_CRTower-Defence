#include <pspctrl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "render.h"
#include "audio.h"
#include "save.h"

#define ICON_CROSS    "\x01"
#define ICON_CIRCLE   "\x02"
#define ICON_SQUARE   "\x03"
#define ICON_TRIANGLE "\x04"

Game g;

#define ACTIVE_ROWS 15
static int active_scroll;

/* ------------------------------------------------------------------ */
/* palette                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t accent, accent_dim, accent_bright, accent_dark, accent_deep;
    uint32_t danger, danger_soft;
    uint32_t bg, bg_deep, bg_panel, bg_hover;
    uint32_t laser, laser_glow, laser_core;
    uint32_t enemy, projectile, cannon_proj, frost_proj, sniper_proj;
} Palette;

static Palette PAL;

static const Palette palettes[3] = {
    /* amber */
    {
        C_RGB(255, 149, 0), C_RGB(204, 119, 0), C_RGB(255, 210, 127),
        C_RGB(255, 96, 48), C_RGB(204, 68, 0),
        C_RGB(255, 48, 0), C_RGB(255, 96, 48),
        C_RGB(16, 6, 0), C_RGB(10, 3, 0), C_RGB(26, 10, 0), C_RGB(53, 21, 0),
        C_RGB(255, 48, 0), C_RGB(255, 170, 136), C_RGB(255, 245, 176),
        C_RGB(255, 149, 0), C_RGB(255, 245, 176), C_RGB(255, 170, 68),
        C_RGB(255, 245, 176), C_RGB(255, 245, 176)
    },
    /* blue */
    {
        C_RGB(120, 190, 240), C_RGB(74, 138, 192), C_RGB(180, 220, 255),
        C_RGB(74, 138, 192), C_RGB(42, 90, 128),
        C_RGB(255, 80, 80), C_RGB(255, 128, 112),
        C_RGB(2, 10, 20), C_RGB(1, 6, 16), C_RGB(10, 26, 42), C_RGB(15, 42, 68),
        C_RGB(255, 48, 96), C_RGB(255, 170, 204), C_RGB(255, 240, 245),
        C_RGB(120, 190, 240), C_RGB(180, 220, 255), C_RGB(160, 208, 255),
        C_RGB(224, 240, 255), C_RGB(208, 232, 255)
    },
    /* green */
    {
        C_RGB(51, 255, 102), C_RGB(26, 153, 68), C_RGB(153, 255, 170),
        C_RGB(26, 153, 68), C_RGB(10, 85, 48),
        C_RGB(255, 48, 0), C_RGB(255, 96, 48),
        C_RGB(0, 16, 0), C_RGB(0, 8, 0), C_RGB(4, 32, 12), C_RGB(10, 48, 24),
        C_RGB(255, 48, 0), C_RGB(255, 170, 136), C_RGB(255, 245, 176),
        C_RGB(51, 255, 102), C_RGB(153, 255, 170), C_RGB(128, 255, 153),
        C_RGB(208, 255, 208), C_RGB(176, 255, 176)
    },
};

/* ------------------------------------------------------------------ */
/* data                                                                */
/* ------------------------------------------------------------------ */

const TowerType tower_types[T_COUNT] = {
    { "ARCHER",  "ARC", 75,  70.0f,  12.0f, 50,  4.5f, 0.0f,  0.00f, 0,   0, 0 },
    { "CANNON",   "CAN", 150, 60.0f,  35.0f, 100, 3.2f, 26.0f, 0.00f, 0,   0, 1 },
    { "FROST", "FRO", 240, 66.0f,  6.0f,  70,  4.0f, 0.0f,  0.17f, 90,  0, 2 },
    { "SNIPER", "SNI", 250, 150.0f, 60.0f, 120, 7.0f, 0.0f,  0.00f, 0,   0, 3 },
    { "LASER",   "LAS", 350, 130.0f, 45.0f, 180, 0.0f, 0.0f,  0.00f, 0,   1, 4 },
};

const Perk perks[P_COUNT] = {
    { P_GOLD_BONUS,      "GOLD VEIN",      "+25% GOLD FROM KILLS", NULL, RARITY_COMMON },
    { P_CASTLE_HP,       "FORTRESS WALLS",     "+30 MAX CASTLE HP AND FULL REPAIR", NULL, RARITY_COMMON },
    { P_ARROW_DMG,       "SHARP ARROWS",     "+20% ARCHER DAMAGE", NULL, RARITY_COMMON },
    { P_ARROW_RATE,      "QUICK SHOT",   "-15% ARCHER RELOAD TIME", NULL, RARITY_COMMON },
    { P_CANNON_DMG,      "HEAVY SHELLS",      "+25% CANNON DAMAGE", NULL, RARITY_COMMON },
    { P_CANNON_SPLASH,   "WIDE BLAST",     "+30% CANNON SPLASH RADIUS", NULL, RARITY_RARE },
    { P_FROST_SLOW,      "DEEP CHILL",    "+15% FROST SLOW STRENGTH", NULL, RARITY_RARE },
    { P_FROST_DURATION,  "LONG WINTER",       "+40% SLOW DURATION", NULL, RARITY_COMMON },
    { P_SNIPER_DMG,      "ARMOR PIERCE",       "+30% SNIPER DAMAGE", NULL, RARITY_COMMON },
    { P_SNIPER_RATE,     "RAPID FIRE",        "-20% SNIPER RELOAD TIME", NULL, RARITY_RARE },
    { P_LASER_DMG,       "FOCUS",             "+25% LASER DAMAGE", NULL, RARITY_COMMON },
    { P_LASER_RATE,      "CORE OVERLOAD",     "-15% LASER RELOAD TIME", NULL, RARITY_RARE },
    { P_LASER_WIDTH,     "WIDE BEAM",       "+50% LASER BEAM WIDTH", NULL, RARITY_LEGENDARY },
    { P_ALL_DMG,         "WAR COUNCIL",     "+10% DAMAGE TO ALL TOWERS", NULL, RARITY_LEGENDARY },
    { P_ALL_RANGE,       "OPTICAL NET",   "+10% RANGE TO ALL TOWERS", NULL, RARITY_RARE },
    { P_DISCOUNT,        "THRIFT",          "-10% NEW TOWER COST", NULL, RARITY_COMMON },
    { P_UPGRADE_DISCOUNT,"ENGINEERS",          "-15% UPGRADE COST", NULL, RARITY_RARE },
    { P_SELL_BONUS,      "RESELLER",        "+15% SELL REFUND", NULL, RARITY_COMMON },
    { P_INTEREST,        "BANK INTEREST","+10% GOLD AT WAVE END", NULL, RARITY_COMMON },
    { P_DOUBLE_SHOT,     "DOUBLE VOLLEY",      "ALL TOWERS FIRE 1.5X FASTER", NULL, RARITY_LEGENDARY },
    { P_GOLD_RUSH,       "GOLD RUSH", "X2 GOLD FROM KILLS, ENEMY HP +15%", NULL, RARITY_LEGENDARY },
    { P_CURSED_FURY,     "CURSED FURY",  "+60% DAMAGE TO ALL TOWERS", "+25% ENEMY HP", RARITY_CURSED },
    { P_CURSED_SPEED,    "ABYSS SPEED",   "TOWERS FIRE 60% FASTER", "+15% ENEMY SPEED", RARITY_CURSED },
    { P_CURSED_GOLD,     "BLOOD GOLD",   "+150% GOLD FROM KILLS", "CASTLE LOSES 20 HP PERMANENTLY", RARITY_CURSED },
    { P_CURSED_RANGE,    "ALL-SEEING EYE",    "+50% RANGE TO ALL TOWERS", "+20% ENEMIES EACH WAVE", RARITY_CURSED },
    { P_CURSED_CANNON,   "HELLFIRE SHELLS",       "+100% CANNON DAMAGE, +80% SPLASH", "-15% CASTLE HP PERMANENTLY", RARITY_CURSED },
    { P_CURSED_LASER,    "PRIDE'S SIN",      "+80% LASER DAMAGE, +100% BEAM WIDTH", "BOSSES APPEAR 3X MORE OFTEN", RARITY_CURSED },
    { P_CURSED_FROST,    "ETERNAL FROST",   "+50% SLOW, +100% DURATION", "+20% ENEMY HP", RARITY_CURSED },
    { P_CURSED_ECONOMY,  "DEVIL'S BARGAIN", "TOWERS 30% CHEAPER, UPGRADES 40%", "-30% GOLD FROM KILLS", RARITY_CURSED },
};

const Modifier modifiers[] = {
    { 0, "+20% ENEMY HP" },
    { 1, "+10% SPEED" },
    { 2, "MORE BOSSES" },
    { 3, "+25% COUNT" },
    { 4, "-20% GOLD" },
    { 5, "HEAVY WAVES" },
    { 6, "ELITE" },
};
const int modifier_count = (int)(sizeof(modifiers) / sizeof(modifiers[0]));

static const float perk_step[P_COUNT] = {
    0.25f, 0.0f, 0.20f, 0.15f, 0.25f, 0.30f, 0.15f, 0.40f, 0.30f, 0.20f,
    0.25f, 0.15f, 0.50f, 0.10f, 0.10f, 0.10f, 0.15f, 0.15f, 0.10f, 1.0f,
    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static unsigned int rng_state = 0x12345678u;

static unsigned int rnd_u32(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static float rndf(void) {
    return (float)(rnd_u32() >> 8) / 16777216.0f;
}

static int rndi(int n) {
    if (n <= 1) return 0;
    return (int)(rnd_u32() % (unsigned int)n);
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Compact HUD numbers: 9999, 10K, 105K, 1M, 1.5M, 10M. */
static void format_amount(float value, char *out, size_t n) {
    if (value < 0.0f) value = 0.0f;
    if (value < 10000.0f) {
        snprintf(out, n, "%d", (int)value);
    } else if (value < 1000000.0f) {
        snprintf(out, n, "%dK", (int)(value / 1000.0f));
    } else if (value < 1000000000.0f) {
        float m = value / 1000000.0f;
        if (m >= 10.0f || m == floorf(m)) snprintf(out, n, "%dM", (int)m);
        else snprintf(out, n, "%.1fM", m);
    } else {
        float b = value / 1000000000.0f;
        if (b >= 10.0f || b == floorf(b)) snprintf(out, n, "%dB", (int)b);
        else snprintf(out, n, "%.1fB", b);
    }
}

static float dist2(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

static float dist(float ax, float ay, float bx, float by) {
    return sqrtf(dist2(ax, ay, bx, by));
}

static uint32_t mix_color(uint32_t bg, uint32_t fg, float a) {
    int br = (int)(bg & 255), bgc = (int)((bg >> 8) & 255), bb = (int)((bg >> 16) & 255);
    int fr = (int)(fg & 255), fgc = (int)((fg >> 8) & 255), fb = (int)((fg >> 16) & 255);
    int r = (int)(br + (fr - br) * a);
    int gg = (int)(bgc + (fgc - bgc) * a);
    int b = (int)(bb + (fb - bb) * a);
    return C_RGB(r, gg, b);
}

static void theme_apply(int t) {
    if (t < 0) t = 0;
    if (t > 2) t = 2;
    g.theme = t;
    PAL = palettes[t];
}

static void sfx(int ev, int gap_ms) {
    if (g.sound_muted) return;
    if (gap_ms > 0 && g.time_ms - g.last_sfx[ev] < (float)gap_ms) return;
    g.last_sfx[ev] = g.time_ms;
    audio_event(ev);
}

static void toast_set(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g.toast, sizeof(g.toast), fmt, ap);
    va_end(ap);
    g.toast_timer = 130;
}

static int path_locked(void) { return g.tower_n > 0; }

static void update_castle(void) {
    if (g.path_n >= 2) {
        g.castle = g.path[g.path_n - 1];
        g.has_castle = 1;
    } else {
        g.has_castle = 0;
        g.castle.x = g.castle.y = 0;
    }
}

static float point_seg_dist2(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay;
    float wx = px - ax, wy = py - ay;
    float len2 = vx * vx + vy * vy;
    if (len2 < 0.0001f) return wx * wx + wy * wy;
    float t = (wx * vx + wy * vy) / len2;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float dx = px - (ax + vx * t);
    float dy = py - (ay + vy * t);
    return dx * dx + dy * dy;
}

static int tower_at(float x, float y) {
    for (int i = 0; i < g.tower_n; i++) {
        if (dist2(x, y, g.towers[i].x, g.towers[i].y) <=
            (TOWER_RADIUS + 4.0f) * (TOWER_RADIUS + 4.0f)) return i;
    }
    return -1;
}


/* ------------------------------------------------------------------ */
/* tower stats and economy                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    int damage, range, fire_rate, splash, slow_duration;
    float slow_factor;
} TowerStats;

static TowerStats tower_stats(const Tower *t) {
    const TowerType *b = &tower_types[t->type];
    float dmg = b->damage * (1.0f + 0.30f * t->level);
    float range = b->range * (1.0f + 0.10f * t->level);
    float rate = b->fire_rate * (1.0f - 0.10f * t->level);
    float splash = b->splash;
    float slow_dur = (float)b->slow_duration;
    float slow = b->slow;

    if (g.perk[P_ALL_DMG]) dmg *= (1.0f + g.perk[P_ALL_DMG]);
    if (g.perk[P_ALL_RANGE]) range *= (1.0f + g.perk[P_ALL_RANGE]);
    if (g.perk[P_DOUBLE_SHOT] > 0.0f)
        rate *= powf(0.667f, g.perk[P_DOUBLE_SHOT]);

    switch (t->type) {
        case T_ARROW:
            if (g.perk[P_ARROW_DMG]) dmg *= (1.0f + g.perk[P_ARROW_DMG]);
            if (g.perk_stacks[P_ARROW_RATE])
                rate *= powf(0.85f, (float)g.perk_stacks[P_ARROW_RATE]);
            break;
        case T_CANNON:
            if (g.perk[P_CANNON_DMG]) dmg *= (1.0f + g.perk[P_CANNON_DMG]);
            if (g.perk[P_CANNON_SPLASH]) splash *= (1.0f + g.perk[P_CANNON_SPLASH]);
            break;
        case T_FROST:
            if (g.perk[P_FROST_SLOW])
                slow = fmaxf(0.02f, slow * (1.0f - g.perk[P_FROST_SLOW]));
            if (g.perk[P_FROST_DURATION]) slow_dur *= (1.0f + g.perk[P_FROST_DURATION]);
            break;
        case T_SNIPER:
            if (g.perk[P_SNIPER_DMG]) dmg *= (1.0f + g.perk[P_SNIPER_DMG]);
            if (g.perk_stacks[P_SNIPER_RATE])
                rate *= powf(0.80f, (float)g.perk_stacks[P_SNIPER_RATE]);
            break;
        case T_LASER:
            if (g.perk[P_LASER_DMG]) dmg *= (1.0f + g.perk[P_LASER_DMG]);
            if (g.perk_stacks[P_LASER_RATE])
                rate *= powf(0.85f, (float)g.perk_stacks[P_LASER_RATE]);
            break;
    }

    TowerStats s;
    s.damage = (int)(dmg + 0.5f);
    s.range = (int)(range + 0.5f);
    s.fire_rate = (int)(rate + 0.5f);
    if (s.fire_rate < 6) s.fire_rate = 6;
    s.splash = (int)(splash + 0.5f);
    s.slow_duration = (int)(slow_dur + 0.5f);
    s.slow_factor = slow;
    return s;
}

static int tower_cost(int type) {
    int base = tower_types[type].cost;
    float d = g.perk[P_DISCOUNT];
    if (d > 0.7f) d = 0.7f;
    return (int)(base * (1.0f - d) + 0.5f);
}

static int upgrade_cost(const Tower *t) {
    float c = (float)tower_types[t->type].cost * 0.65f * (float)(t->level + 1);
    float d = g.perk[P_UPGRADE_DISCOUNT];
    if (d > 0.8f) d = 0.8f;
    c *= (1.0f - d);
    return (int)(c + 0.5f);
}

static int sell_refund(const Tower *t) {
    float base = 0.7f + g.perk[P_SELL_BONUS];
    if (base > 1.0f) base = 1.0f;
    return (int)((float)(tower_types[t->type].cost + t->invested) * base);
}

/* ------------------------------------------------------------------ */
/* effects                                                             */
/* ------------------------------------------------------------------ */

static void spawn_particles(float x, float y, uint32_t color, int count) {
    int room = PARTICLE_MAX - g.particle_n;
    if (room <= 0) return;
    if (count > room) count = room;
    for (int i = 0; i < count; i++) {
        float a = rndf() * 6.2831853f;
        float sp = 0.6f + rndf() * 1.8f;
        Particle *p = &g.particles[g.particle_n++];
        p->x = x; p->y = y;
        p->vx = cosf(a) * sp;
        p->vy = sinf(a) * sp;
        p->life = 20 + (int)(rndf() * 20);
        p->max_life = p->life;
        p->color = color;
        p->size = 0.8f + rndf() * 1.7f;
    }
}

static void push_beam(float x1, float y1, float x2, float y2, uint32_t color,
                      uint32_t glow, float width) {
    if (g.beam_n >= BEAM_MAX) {
        memmove(&g.beams[0], &g.beams[1], sizeof(Beam) * (BEAM_MAX - 1));
        g.beam_n = BEAM_MAX - 1;
    }
    Beam *b = &g.beams[g.beam_n++];
    b->x1 = x1; b->y1 = y1; b->x2 = x2; b->y2 = y2;
    b->life = 10; b->max_life = 10;
    b->color = color; b->glow = glow;
    b->width = width;
}

static void remove_enemy(int i) {
    if (i < 0 || i >= g.enemy_n) return;
    memmove(&g.enemies[i], &g.enemies[i + 1],
            sizeof(Enemy) * (g.enemy_n - i - 1));
    g.enemy_n--;
}

static void remove_tower(int i) {
    if (i < 0 || i >= g.tower_n) return;
    memmove(&g.towers[i], &g.towers[i + 1],
            sizeof(Tower) * (g.tower_n - i - 1));
    g.tower_n--;
}

/* ------------------------------------------------------------------ */
/* perks and modifiers                                                 */
/* ------------------------------------------------------------------ */

static void perk_apply(int id) {
    g.perk_stacks[id]++;

    switch (id) {
        case P_CASTLE_HP:
            g.castle_max_hp += 30.0f;
            g.castle_hp = g.castle_max_hp;
            break;
        case P_GOLD_RUSH:
            g.perk[P_GOLD_RUSH] += 1.0f;
            g.enemy_hp_from_perk += 0.15f;
            break;
        case P_CURSED_FURY:
            g.perk[P_ALL_DMG] += 0.60f;
            g.enemy_hp_from_perk += 0.25f;
            break;
        case P_CURSED_SPEED:
            g.perk[P_DOUBLE_SHOT] += 1.0f;
            g.mod_enemy_speed *= 1.15f;
            break;
        case P_CURSED_GOLD:
            g.perk[P_GOLD_BONUS] += 1.50f;
            g.castle_max_hp -= 20.0f;
            if (g.castle_max_hp < 50.0f) g.castle_max_hp = 50.0f;
            if (g.castle_hp > g.castle_max_hp) g.castle_hp = g.castle_max_hp;
            if (g.castle_hp <= 0.0f) g.castle_hp = 1.0f;
            break;
        case P_CURSED_RANGE:
            g.perk[P_ALL_RANGE] += 0.50f;
            g.mod_count *= 1.20f;
            break;
        case P_CURSED_CANNON:
            g.perk[P_CANNON_DMG] += 1.00f;
            g.perk[P_CANNON_SPLASH] += 0.80f;
            g.castle_max_hp = floorf(g.castle_max_hp * 0.85f);
            if (g.castle_max_hp < 50.0f) g.castle_max_hp = 50.0f;
            if (g.castle_hp > g.castle_max_hp) g.castle_hp = g.castle_max_hp;
            break;
        case P_CURSED_LASER:
            g.perk[P_LASER_DMG] += 0.80f;
            g.perk[P_LASER_WIDTH] += 1.00f;
            g.mod_boss_chance += 0.30f;
            break;
        case P_CURSED_FROST:
            g.perk[P_FROST_SLOW] += 0.50f;
            g.perk[P_FROST_DURATION] += 1.00f;
            g.enemy_hp_from_perk += 0.20f;
            break;
        case P_CURSED_ECONOMY:
            g.perk[P_DISCOUNT] += 0.30f;
            g.perk[P_UPGRADE_DISCOUNT] += 0.40f;
            g.mod_reward *= 0.70f;
            break;
        default:
            g.perk[id] += perk_step[id];
            break;
    }
}

static void mod_apply(int id) {
    switch (id) {
        case 0: g.mod_enemy_hp *= 1.20f; break;
        case 1: g.mod_enemy_speed *= 1.10f; break;
        case 2: g.mod_boss_chance += 0.15f; break;
        case 3: g.mod_count *= 1.25f; break;
        case 4: g.mod_reward *= 0.80f; break;
        case 5: g.mod_enemy_hp *= 1.15f; g.mod_enemy_speed *= 0.90f; break;
        case 6: g.mod_boss_hp *= 1.50f; break;
    }
}

static void add_random_modifier(void) {
    int used[16];
    memset(used, 0, sizeof(used));
    for (int i = 0; i < g.mod_n; i++) used[g.active_mods[i]] = 1;
    int avail[16], an = 0;
    for (int i = 0; i < modifier_count; i++)
        if (!used[i]) avail[an++] = i;
    if (an == 0) return;
    int id = avail[rndi(an)];
    g.active_mods[g.mod_n++] = id;
    mod_apply(id);
    sfx(AE_MOD, 0);
    toast_set("MODIFIER: %s", modifiers[id].name);
    g.mod_banner = 180;
}

static void shuffle_ids(int *a, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rndi(i + 1);
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

static void pick_perk_choices(void) {
    int regular[P_COUNT], rn = 0;
    int cursed[P_COUNT], cn = 0;
    for (int i = 0; i < P_COUNT; i++) {
        if (perks[i].rarity == RARITY_CURSED) cursed[cn++] = i;
        else regular[rn++] = i;
    }
    shuffle_ids(regular, rn);
    shuffle_ids(cursed, cn);

    int picked[3], pn = 0;
    if (rndf() < 0.25f && cn > 0) picked[pn++] = cursed[0];

    for (int i = 0; i < rn && pn < 3; i++) {
        int id = regular[i];
        float chance = 1.0f;
        if (perks[id].rarity == RARITY_RARE) chance = 0.60f;
        if (perks[id].rarity == RARITY_LEGENDARY) chance = 0.30f;
        if (rndf() < chance) picked[pn++] = id;
    }
    while (pn < 3) {
        int used[P_COUNT];
        memset(used, 0, sizeof(used));
        for (int i = 0; i < pn; i++) used[picked[i]] = 1;
        int rem[P_COUNT], remn = 0;
        for (int i = 0; i < rn; i++)
            if (!used[regular[i]]) rem[remn++] = regular[i];
        if (remn == 0) break;
        picked[pn++] = rem[rndi(remn)];
    }
    while (pn < 3) picked[pn++] = regular[rndi(rn)];

    shuffle_ids(picked, 3);
    for (int i = 0; i < 3; i++) g.perk_choices[i] = picked[i];
    g.perk_sel = 0;
}

static int reroll_cost(void) {
    if (!g.reroll_used) return 0;
    return (int)(100.0f * powf(1.5f, (float)(g.reroll_count - 1)) + 0.5f);
}

static void open_perk_modal(void) {
    g.pending_perk = 1;
    g.reroll_count = 0;
    g.reroll_used = 0;
    pick_perk_choices();
    g.drawing = 0;
    sfx(AE_PERK, 0);
}

static void choose_perk(int id) {
    perk_apply(id);
    g.pending_perk = 0;
    if (perks[id].rarity == RARITY_CURSED) {
        sfx(AE_CURSED, 0);
        toast_set("CURSE: %s", perks[id].name);
    } else {
        sfx(AE_PERK, 0);
        toast_set("PERK: %s", perks[id].name);
    }
}

static void reroll_perks(void) {
    int cost = reroll_cost();
    if (cost > 0) {
        if (g.gold < cost) { sfx(AE_UPGRADE_FAIL, 0); return; }
        g.gold -= cost;
    }
    g.reroll_used = 1;
    g.reroll_count++;
    pick_perk_choices();
    sfx(AE_REROLL, 0);
}

/* ------------------------------------------------------------------ */
/* building / upgrading / selling                                      */
/* ------------------------------------------------------------------ */

static int can_place(float x, float y) {
    float margin = TOWER_RADIUS + 3.0f;
    if (x < FIELD_LEFT + margin || x > FIELD_RIGHT - margin ||
        y < FIELD_TOP + margin || y > FIELD_BOTTOM - margin) return 0;

    float clear = TOWER_RADIUS + 8.0f;
    float clear2 = clear * clear;
    for (int i = 0; i < g.path_n; i++) {
        if (dist2(x, y, g.path[i].x, g.path[i].y) < clear2) return 0;
    }
    for (int i = 0; i + 1 < g.path_n; i++) {
        if (point_seg_dist2(x, y, g.path[i].x, g.path[i].y,
                            g.path[i + 1].x, g.path[i + 1].y) < clear2) return 0;
    }

    if (g.has_castle) {
        float cd = CASTLE_RADIUS + TOWER_RADIUS + 3.0f;
        if (dist2(x, y, g.castle.x, g.castle.y) < cd * cd) return 0;
    }

    for (int i = 0; i < g.tower_n; i++) {
        float d = 2.0f * TOWER_RADIUS + 2.0f;
        if (dist2(x, y, g.towers[i].x, g.towers[i].y) < d * d) return 0;
    }
    return 1;
}

static float angle_to_nearest_path(float x, float y) {
    float best = 1e18f;
    float ang = -1.5707963f;
    for (int i = 0; i < g.path_n; i++) {
        float d = dist2(x, y, g.path[i].x, g.path[i].y);
        if (d < best) {
            best = d;
            ang = atan2f(g.path[i].y - y, g.path[i].x - x);
        }
    }
    return ang;
}

static void try_build_tower(int type, float x, float y) {
    if (g.path_n < 2) {
        toast_set("DRAW THE PATH FIRST");
        sfx(AE_PLACE_ERROR, 0);
        return;
    }
    if (g.tower_n >= TOWER_MAX) {
        toast_set("TOWER LIMIT");
        sfx(AE_PLACE_ERROR, 0);
        return;
    }
    int cost = tower_cost(type);
    if (g.gold < (float)cost) {
        toast_set("NOT ENOUGH GOLD: $%d", cost);
        sfx(AE_PLACE_ERROR, 0);
        return;
    }
    if (!can_place(x, y)) {
        spawn_particles(x, y, PAL.danger, 2);
        sfx(AE_PLACE_ERROR, 0);
        return;
    }
    Tower *t = &g.towers[g.tower_n++];
    t->type = type;
    t->x = x; t->y = y;
    t->angle = angle_to_nearest_path(x, y);
    t->cooldown = 0;
    t->level = 0;
    t->invested = 0;
    g.gold -= (float)cost;
    spawn_particles(x, y, PAL.accent_bright, 3);
    sfx(AE_BUILD, 0);
    g.mode = MODE_BUILD;
    g.selected_tower = -1;
    toast_set("%s READY", tower_types[type].name);
}

static void quick_upgrade(int idx) {
    if (idx < 0 || idx >= g.tower_n) return;
    Tower *t = &g.towers[idx];
    if (t->level >= MAX_LEVEL) {
        sfx(AE_UPGRADE_FAIL, 0);
        toast_set("MAX LEVEL");
        return;
    }
    int cost = upgrade_cost(t);
    if (g.gold < (float)cost) {
        sfx(AE_UPGRADE_FAIL, 0);
        toast_set("NEED $%d", cost);
        return;
    }
    g.gold -= (float)cost;
    t->level++;
    t->invested += cost;
    spawn_particles(t->x, t->y, PAL.accent_bright, 4);
    if (g.flash_n < FLASH_MAX) {
        UpgradeFlash *f = &g.flashes[g.flash_n++];
        f->x = t->x; f->y = t->y;
        f->life = 30; f->max_life = 30;
    }
    sfx(AE_UPGRADE, 0);
    toast_set("%s LVL %d", tower_types[t->type].name, t->level);
}

static void sell_tower(int idx) {
    if (idx < 0 || idx >= g.tower_n) return;
    Tower t = g.towers[idx];
    int refund = sell_refund(&t);
    g.gold += (float)refund;
    spawn_particles(t.x, t.y, PAL.accent_bright, 4);
    sfx(AE_SELL, 0);
    remove_tower(idx);
    if (g.selected_tower == idx) g.selected_tower = -1;
    else if (g.selected_tower > idx) g.selected_tower--;
    g.mode = MODE_BUILD;
    g.sell_hold = 0;
    toast_set("SOLD: +$%d", refund);
}

/* ------------------------------------------------------------------ */
/* waves and combat                                                    */
/* ------------------------------------------------------------------ */

static int spawn_delay(int wave) {
    int w = wave - 1;
    if (w < 0) w = 0;
    int d = (int)(40.0f * expf(-(float)w / 35.0f) + 0.5f);
    if (d < 2) d = 2;
    return d;
}

static int spawn_batch(int wave) {
    if (wave <= 10) return 1;
    if (wave <= 25) return 2;
    if (wave <= 50) return 3;
    if (wave <= 75) return 5;
    if (wave <= 100) return 8;
    if (wave <= 150) return 14;
    return 20;
}

static int spawn_enemy(void) {
    if (g.enemy_n >= ENEMY_MAX || g.path_n < 2) return 0;

    float base_hp = 15.0f + (float)g.wave * 42.0f +
                    (float)g.wave * (float)g.wave * 0.55f;
    float hp = base_hp * g.mod_enemy_hp;
    if (g.enemy_hp_from_perk > 0.0f) hp *= (1.0f + g.enemy_hp_from_perk);

    float speed = (0.34f + (float)g.wave * 0.035f) * g.mod_enemy_speed;
    float radius = ENEMY_RADIUS;
    float reward = (14.0f + (float)g.wave * 2.2f) / 2.0f * g.mod_reward;
    if (g.perk[P_GOLD_BONUS]) reward *= (1.0f + g.perk[P_GOLD_BONUS]);
    if (g.perk[P_GOLD_RUSH] > 0.0f)
        reward *= powf(2.0f, g.perk[P_GOLD_RUSH]);

    int is_boss = 0;
    float boss_chance = 0.15f + g.mod_boss_chance;
    if (g.wave % 5 == 0 && rndf() < boss_chance) {
        hp *= 4.0f;
        hp *= g.mod_boss_hp;
        speed *= 0.70f;
        radius = ENEMY_RADIUS + 3.0f;
        reward *= 5.0f;
        is_boss = 1;
    } else if (rndf() < 0.25f) {
        hp *= 0.60f;
        speed *= 1.60f;
        radius = ENEMY_RADIUS - 1.5f;
        reward *= 0.80f;
    }

    Enemy *e = &g.enemies[g.enemy_n++];
    e->x = g.path[0].x;
    e->y = g.path[0].y;
    e->hp = hp;
    e->max_hp = hp;
    e->speed = speed;
    e->radius = radius;
    e->target_index = 1;
    e->slow_factor = 1.0f;
    e->slow_timer = 0;
    e->wobble = rndf() * 6.2831853f;
    e->reward = (int)(reward + 0.5f);
    if (e->reward < 1) e->reward = 1;
    e->is_boss = is_boss;
    return 1;
}

static void start_wave(void) {
    if (g.game_over || g.wave_active || g.pending_perk) return;
    if (g.path_n < 2) {
        toast_set("DRAW THE PATH FIRST");
        sfx(AE_PLACE_ERROR, 0);
        return;
    }
    g.wave++;
    g.wave_active = 1;
    g.enemies_to_spawn = (int)(5.0f * powf(1.05f, (float)g.wave) * g.mod_count + 0.5f);
    if (g.enemies_to_spawn < 1) g.enemies_to_spawn = 1;
    g.enemies_total = g.enemies_to_spawn;
    g.spawn_timer = 0;
    g.wave_banner = 120;
    sfx(AE_WAVE_START, 0);
}

static void fire_laser(Tower *t, TowerStats *st) {
    float ca = cosf(t->angle), sa = sinf(t->angle);
    float sx = t->x + ca * 12.0f;
    float sy = t->y + sa * 12.0f;
    float ex = t->x + ca * (float)st->range;
    float ey = t->y + sa * (float)st->range;

    float line_w = 8.0f * (1.0f + g.perk[P_LASER_WIDTH]);
    for (int i = 0; i < g.enemy_n; i++) {
        Enemy *e = &g.enemies[i];
        float px = e->x - t->x;
        float py = e->y - t->y;
        float proj = px * ca + py * sa;
        if (proj < 0.0f || proj > (float)st->range) continue;
        float perp = fabsf(-px * sa + py * ca);
        if (perp <= line_w) {
            e->hp -= (float)st->damage;
            spawn_particles(e->x, e->y, PAL.laser, 1);
        }
    }
    for (int i = g.enemy_n - 1; i >= 0; i--) {
        if (g.enemies[i].hp <= 0.0f) {
            Enemy *e = &g.enemies[i];
            g.gold += (float)e->reward;
            spawn_particles(e->x, e->y, PAL.accent, 2);
            if (e->is_boss) sfx(AE_BOSSDIE, 300);
            else sfx(AE_DIE, 50);
            remove_enemy(i);
        }
    }

    float visw = 2.0f + g.perk[P_LASER_WIDTH] * 8.0f;
    push_beam(sx, sy, ex, ey, PAL.laser, PAL.laser_glow, visw);
    sfx(AE_LASER, 60);
}

static uint32_t projectile_color(int type) {
    switch (type) {
        case T_CANNON: return PAL.cannon_proj;
        case T_FROST: return PAL.frost_proj;
        case T_SNIPER: return PAL.sniper_proj;
        default: return PAL.projectile;
    }
}

static void update_world(float dt) {
    (void)dt;

    if (g.wave_active && g.enemies_to_spawn > 0) {
        g.spawn_timer--;
        if (g.spawn_timer <= 0) {
            int batch = spawn_batch(g.wave);
            for (int i = 0; i < batch && g.enemies_to_spawn > 0; i++) {
                if (!spawn_enemy()) break;
                g.enemies_to_spawn--;
            }
            g.spawn_timer = spawn_delay(g.wave);
            if (g.enemies_to_spawn > 0 && g.enemy_n >= ENEMY_MAX)
                g.spawn_timer = 4;
        }
    }

    if (g.wave_active && g.enemies_to_spawn == 0 && g.enemy_n == 0) {
        g.wave_active = 0;
        float bonus = 40.0f + (float)g.wave * 6.0f;
        if (g.perk[P_INTEREST]) bonus *= (1.0f + g.perk[P_INTEREST]);
        g.gold += floorf(bonus + 0.5f);
        sfx(AE_WAVE_END, 0);
        toast_set("WAVE %d CLEARED  +$%d", g.wave, (int)(bonus + 0.5f));

        if (g.wave > 0 && g.wave % 5 == 0 && g.last_perk_wave != g.wave) {
            g.last_perk_wave = g.wave;
            open_perk_modal();
        }
        if (g.wave > 0 && g.wave % 10 == 0 && g.last_mod_wave != g.wave) {
            g.last_mod_wave = g.wave;
            add_random_modifier();
        }
        return;
    }

    for (int i = g.enemy_n - 1; i >= 0; i--) {
        Enemy *e = &g.enemies[i];
        if (e->slow_timer > 0) {
            e->slow_timer--;
            if (e->slow_timer <= 0) e->slow_factor = 1.0f;
        }
        if (g.path_n < 2) continue;

        int ti = e->target_index;
        if (ti >= g.path_n) ti = g.path_n - 1;
        float tx = g.path[ti].x, ty = g.path[ti].y;
        float d = dist(e->x, e->y, tx, ty);
        if (d < 4.0f) {
            e->target_index++;
            if (e->target_index >= g.path_n) {
                if (g.has_castle) {
                    float dmg = e->max_hp * 0.2f;
                    if (dmg < 5.0f) dmg = 5.0f;
                    g.castle_hp -= dmg;
                    spawn_particles(g.castle.x, g.castle.y, PAL.danger, 2);
                    sfx(AE_CASTLEHIT, 120);
                }
                remove_enemy(i);
            }
        } else {
            float sp = e->speed * e->slow_factor;
            e->x += (tx - e->x) / d * sp;
            e->y += (ty - e->y) / d * sp;
            e->wobble += 0.2f;
        }
    }

    for (int ti = 0; ti < g.tower_n; ti++) {
        Tower *t = &g.towers[ti];
        t->cooldown--;
        TowerStats st = tower_stats(t);
        int closest = -1;
        float best = (float)st.range;
        for (int i = 0; i < g.enemy_n; i++) {
            float d = dist(t->x, t->y, g.enemies[i].x, g.enemies[i].y);
            if (d < best) { best = d; closest = i; }
        }
        if (closest < 0) continue;
        Enemy *target = &g.enemies[closest];
        t->angle = atan2f(target->y - t->y, target->x - t->x);

        if (t->cooldown > 0) continue;
        t->cooldown = st.fire_rate;

        if (tower_types[t->type].laser) {
            fire_laser(t, &st);
        } else {
            if (g.proj_n < PROJ_MAX) {
                Projectile *p = &g.projs[g.proj_n++];
                float ca = cosf(t->angle), sa = sinf(t->angle);
                p->x = t->x + ca * 10.0f;
                p->y = t->y + sa * 10.0f;
                p->vx = ca * tower_types[t->type].proj_speed;
                p->vy = sa * tower_types[t->type].proj_speed;
                p->damage = st.damage;
                p->life = 120;
                p->type = t->type;
                p->splash = (float)st.splash;
                p->slow = st.slow_factor;
                p->slow_duration = st.slow_duration;
                p->color = projectile_color(t->type);
            }
            int ev = AE_ARROW;
            if (t->type == T_CANNON) ev = AE_CANNON;
            else if (t->type == T_FROST) ev = AE_FROST;
            else if (t->type == T_SNIPER) ev = AE_SNIPER;
            sfx(ev, ev == AE_FROST ? 90 : 60);
        }
    }

    for (int i = g.beam_n - 1; i >= 0; i--) {
        g.beams[i].life--;
        if (g.beams[i].life <= 0) {
            memmove(&g.beams[i], &g.beams[i + 1],
                    sizeof(Beam) * (g.beam_n - i - 1));
            g.beam_n--;
        }
    }
    for (int i = g.flash_n - 1; i >= 0; i--) {
        g.flashes[i].life--;
        if (g.flashes[i].life <= 0) {
            memmove(&g.flashes[i], &g.flashes[i + 1],
                    sizeof(UpgradeFlash) * (g.flash_n - i - 1));
            g.flash_n--;
        }
    }

    for (int i = g.proj_n - 1; i >= 0; i--) {
        Projectile *p = &g.projs[i];
        p->x += p->vx;
        p->y += p->vy;
        p->life--;

        int hit = 0;
        for (int j = g.enemy_n - 1; j >= 0; j--) {
            Enemy *e = &g.enemies[j];
            float rr = e->radius + 3.0f;
            if (dist2(p->x, p->y, e->x, e->y) >= rr * rr) continue;

            e->hp -= (float)p->damage;

            if (p->splash > 0.0f) {
                float s2 = p->splash * p->splash;
                for (int k = g.enemy_n - 1; k >= 0; k--) {
                    if (k == j) continue;
                    Enemy *o = &g.enemies[k];
                    if (dist2(p->x, p->y, o->x, o->y) < s2) {
                        o->hp -= (float)p->damage * 0.5f;
                        if (o->hp <= 0.0f) {
                            g.gold += (float)o->reward;
                            spawn_particles(o->x, o->y, PAL.accent, 2);
                            sfx(AE_DIE, 50);
                            remove_enemy(k);
                            if (k < j) j--;
                        }
                    }
                }
                e = &g.enemies[j];
                spawn_particles(p->x, p->y, PAL.cannon_proj, 1);
            }

            if (p->slow > 0.0f) {
                e->slow_factor = p->slow;
                e->slow_timer = p->slow_duration;
            }

            if (e->hp <= 0.0f) {
                g.gold += (float)e->reward;
                spawn_particles(e->x, e->y, PAL.accent, 2);
                if (e->is_boss) sfx(AE_BOSSDIE, 300);
                else sfx(AE_DIE, 50);
                remove_enemy(j);
            } else {
                spawn_particles(e->x, e->y, p->color, 1);
                sfx(AE_HIT, 40);
            }
            hit = 1;
            break;
        }

        if (hit || p->life <= 0 || p->x < 0 || p->x > GAME_W ||
            p->y < 0 || p->y > GAME_H) {
            g.projs[i] = g.projs[g.proj_n - 1];
            g.proj_n--;
        }
    }

    for (int i = g.particle_n - 1; i >= 0; i--) {
        Particle *p = &g.particles[i];
        p->x += p->vx;
        p->y += p->vy;
        p->vx *= 0.95f;
        p->vy *= 0.95f;
        p->life--;
        if (p->life <= 0) {
            g.particles[i] = g.particles[g.particle_n - 1];
            g.particle_n--;
        }
    }

    if (g.castle_hp <= 0.0f && !g.game_over) {
        g.castle_hp = 0;
        g.game_over = 1;
        g.pending_perk = 0;
        g.paused = 0;
        sfx(AE_GAMEOVER, 0);
    }
}

/* ------------------------------------------------------------------ */
/* input                                                               */
/* ------------------------------------------------------------------ */

static int active_perk_count(void) {
    int n = 0;
    for (int i = 0; i < P_COUNT; i++) if (g.perk_stacks[i]) n++;
    return n;
}

static int active_cursed_count(void) {
    int n = 0;
    for (int i = 0; i < P_COUNT; i++)
        if (g.perk_stacks[i] && perks[i].rarity == RARITY_CURSED) n++;
    return n;
}

static int active_line_count(void) {
    int nperks = active_perk_count();
    int total = 0;
    if (nperks > 0) total += 1 + nperks;
    if (g.mod_n > 0) total += 1 + g.mod_n;
    return total;
}

static void pause_input(uint32_t buttons, uint32_t pressed) {
    if (pressed & (PSP_CTRL_START | PSP_CTRL_SELECT)) {
        g.paused = 0;
        sfx(AE_CLICK, 0);
        return;
    }
    if (g.pause_help == 1) {
        if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS)) {
            g.pause_help = 0;
            sfx(AE_CLICK, 0);
        }
        return;
    }
    if (g.pause_help == 2) {
        int max_scroll = active_line_count() - ACTIVE_ROWS;
        if (max_scroll < 0) max_scroll = 0;
        if (pressed & PSP_CTRL_UP) {
            if (active_scroll > 0) active_scroll--;
            sfx(AE_CLICK, 0);
        }
        if (pressed & PSP_CTRL_DOWN) {
            if (active_scroll < max_scroll) active_scroll++;
            sfx(AE_CLICK, 0);
        }
        if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS)) {
            g.pause_help = 0;
            sfx(AE_CLICK, 0);
        }
        return;
    }
    if (pressed & PSP_CTRL_UP) {
        g.pause_sel = (g.pause_sel + 8) % 9;
        sfx(AE_CLICK, 0);
    }
    if (pressed & PSP_CTRL_DOWN) {
        g.pause_sel = (g.pause_sel + 1) % 9;
        sfx(AE_CLICK, 0);
    }
    if (g.pause_sel == 6 && (pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT))) {
        int d = (pressed & PSP_CTRL_RIGHT) ? 10 : -10;
        g.cursor_speed_pct += d;
        if (g.cursor_speed_pct < 0) g.cursor_speed_pct = 0;
        if (g.cursor_speed_pct > 100) g.cursor_speed_pct = 100;
        sfx(AE_CLICK, 0);
    }
    if (pressed & PSP_CTRL_CROSS) {
        switch (g.pause_sel) {
            case 0:
                g.paused = 0;
                sfx(AE_CLICK, 0);
                break;
            case 1:
                if (save_request_save()) sfx(AE_CLICK, 0);
                else sfx(AE_PLACE_ERROR, 0);
                break;
            case 2:
                if (save_request_load()) sfx(AE_CLICK, 0);
                else sfx(AE_PLACE_ERROR, 0);
                break;
            case 3:
                game_reset();
                break;
            case 4:
                theme_apply((g.theme + 1) % 3);
                sfx(AE_THEME, 0);
                break;
            case 5:
                g.sound_muted = !g.sound_muted;
                audio_set_muted(g.sound_muted);
                if (!g.sound_muted) sfx(AE_CLICK, 0);
                break;
            case 6:
                /* Cross cycles downward; LEFT/RIGHT adjust by 10%. */
                g.cursor_speed_pct -= 10;
                if (g.cursor_speed_pct < 0) g.cursor_speed_pct = 100;
                sfx(AE_CLICK, 0);
                break;
            case 7:
                g.pause_help = 2;
                active_scroll = 0;
                sfx(AE_CLICK, 0);
                break;
            case 8:
                g.pause_help = 1;
                sfx(AE_CLICK, 0);
                break;
        }
    }
    if (pressed & PSP_CTRL_CIRCLE) {
        g.paused = 0;
        sfx(AE_CLICK, 0);
    }
    (void)buttons;
}

static void perk_input(uint32_t pressed) {
    if (pressed & PSP_CTRL_LEFT) {
        g.perk_sel = (g.perk_sel + 2) % 3;
        sfx(AE_CLICK, 0);
    }
    if (pressed & PSP_CTRL_RIGHT) {
        g.perk_sel = (g.perk_sel + 1) % 3;
        sfx(AE_CLICK, 0);
    }
    if (pressed & PSP_CTRL_CROSS) {
        choose_perk(g.perk_choices[g.perk_sel]);
    }
    if (pressed & PSP_CTRL_SQUARE) {
        reroll_perks();
    }
}

static void normal_input(uint32_t buttons, uint32_t pressed) {
    /* tower type selection / rotation */
    if (pressed & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
        int d = (pressed & PSP_CTRL_RTRIGGER) ? 1 : -1;
        g.build_type = (g.build_type + d + T_COUNT) % T_COUNT;
        if (g.path_n >= 2) g.mode = MODE_BUILD;
        sfx(AE_CLICK, 0);
        toast_set("%s  $%d", tower_types[g.build_type].name,
                  tower_cost(g.build_type));
    }

    /* mode toggle / sell hold */
    if (g.mode == MODE_SELECT && g.selected_tower >= 0) {
        if (buttons & PSP_CTRL_SQUARE) {
            g.sell_hold++;
            if (g.sell_hold >= 30) sell_tower(g.selected_tower);
        } else {
            g.sell_hold = 0;
        }
    } else if (pressed & PSP_CTRL_SQUARE) {
        if (g.path_n >= 2) {
            if (g.mode == MODE_BUILD) {
                g.mode = MODE_DRAW;
                g.selected_tower = -1;
                g.sell_hold = 0;
                toast_set(path_locked() ? "FREE CURSOR" : "DRAWING PATH");
                sfx(AE_CLICK, 0);
            } else {
                g.mode = MODE_BUILD;
                g.selected_tower = -1;
                toast_set("BUILD: %s", tower_types[g.build_type].name);
                sfx(AE_CLICK, 0);
            }
        } else {
            toast_set("DRAW THE PATH FIRST");
            sfx(AE_PLACE_ERROR, 0);
        }
    }

    /* quick upgrade */
    if (pressed & PSP_CTRL_TRIANGLE) {
        int idx = g.selected_tower;
        if (idx < 0) idx = tower_at(g.cursor_x, g.cursor_y);
        quick_upgrade(idx);
    }

    /* start wave */
    if (pressed & PSP_CTRL_START) start_wave();

    /* primary action */
    if (pressed & PSP_CTRL_CROSS) {
        int ti = tower_at(g.cursor_x, g.cursor_y);
        if (g.mode == MODE_BUILD) {
            if (ti >= 0) {
                g.selected_tower = ti;
                g.mode = MODE_SELECT;
                g.sell_hold = 0;
                sfx(AE_CLICK, 0);
            } else {
                try_build_tower(g.build_type, g.cursor_x, g.cursor_y);
            }
        } else if (g.mode == MODE_SELECT) {
            if (ti >= 0) {
                g.selected_tower = ti;
                g.sell_hold = 0;
                sfx(AE_CLICK, 0);
            } else {
                g.selected_tower = -1;
                g.mode = path_locked() ? MODE_BUILD : MODE_DRAW;
                sfx(AE_CLICK, 0);
            }
        } else {
            if (ti >= 0) {
                g.selected_tower = ti;
                g.mode = MODE_SELECT;
                g.sell_hold = 0;
                sfx(AE_CLICK, 0);
            } else if (!path_locked() && g.path_n < PATH_MAX_POINTS) {
                if (g.path_n == 0 ||
                    dist(g.path[g.path_n - 1].x, g.path[g.path_n - 1].y,
                         g.cursor_x, g.cursor_y) > 4.0f) {
                    g.path[g.path_n].x = g.cursor_x;
                    g.path[g.path_n].y = g.cursor_y;
                    g.path_n++;
                    update_castle();
                }
                g.drawing = 1;
            } else if (path_locked()) {
                g.mode = MODE_BUILD;
                sfx(AE_CLICK, 0);
            }
        }
    }

    /* secondary / cancel */
    if (pressed & PSP_CTRL_CIRCLE) {
        if (g.selected_tower >= 0) {
            g.selected_tower = -1;
            g.mode = path_locked() ? MODE_BUILD : MODE_DRAW;
            g.sell_hold = 0;
            sfx(AE_CLICK, 0);
        } else if (g.mode == MODE_DRAW && !path_locked() && g.path_n > 0) {
            int best = -1;
            float bestd = 10.0f * 10.0f;
            for (int i = 0; i < g.path_n; i++) {
                float d = dist2(g.path[i].x, g.path[i].y, g.cursor_x, g.cursor_y);
                if (d < bestd) { bestd = d; best = i; }
            }
            if (best < 0) best = g.path_n - 1;
            memmove(&g.path[best], &g.path[best + 1],
                    sizeof(Vec2) * (g.path_n - best - 1));
            g.path_n--;
            update_castle();
            sfx(AE_CLICK, 0);
            toast_set("PATH POINT REMOVED");
        } else if (g.mode == MODE_BUILD && !path_locked()) {
            g.mode = MODE_DRAW;
            sfx(AE_CLICK, 0);
        } else {
            sfx(AE_CLICK, 0);
        }
    }

    /* hold X to trace the path */
    if (g.drawing) {
        if (!(buttons & PSP_CTRL_CROSS)) {
            g.drawing = 0;
        } else if (!path_locked() && g.mode == MODE_DRAW && g.path_n > 0) {
            if (g.path_n < PATH_MAX_POINTS &&
                dist(g.path[g.path_n - 1].x, g.path[g.path_n - 1].y,
                     g.cursor_x, g.cursor_y) > 8.0f) {
                g.path[g.path_n].x = g.cursor_x;
                g.path[g.path_n].y = g.cursor_y;
                g.path_n++;
                update_castle();
            }
        }
    }
}

void game_update(float dt, uint32_t buttons, uint32_t pressed, float mx, float my) {
    g.frame++;
    g.time_ms += dt * 1000.0f;
    if (g.toast_timer > 0) g.toast_timer--;
    if (g.wave_banner > 0) g.wave_banner--;
    /* Hold the modifier popup while a perk choice or the pause menu covers it. */
    if (g.mod_banner > 0 && !g.pending_perk && !g.paused && !g.game_over)
        g.mod_banner--;

    if (pressed & PSP_CTRL_SELECT) {
        if (!g.game_over) {
            g.paused = !g.paused;
            g.pause_sel = 0;
            g.pause_help = 0;
            g.drawing = 0;
            sfx(AE_CLICK, 0);
            return;
        }
    }
    if (g.paused) {
        pause_input(buttons, pressed);
        return;
    }
    if (g.game_over) {
        if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) game_reset();
        return;
    }
    if (g.pending_perk) {
        perk_input(pressed);
        return;
    }

    int speed_pct = g.cursor_speed_pct;
    if (speed_pct < 0) speed_pct = 0;
    if (speed_pct > 100) speed_pct = 100;
    float speed_scale = (speed_pct == 0) ? 0.001f : (float)speed_pct / 100.0f;
    float sp = 125.0f * speed_scale * dt;
    g.cursor_x = clampf(g.cursor_x + mx * sp, FIELD_LEFT, FIELD_RIGHT);
    g.cursor_y = clampf(g.cursor_y + my * sp, FIELD_TOP + 2.0f, FIELD_BOTTOM - 2.0f);

    normal_input(buttons, pressed);
    update_world(dt);
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */
/* ------------------------------------------------------------------ */

static void draw_grid(void) {
    uint32_t c = mix_color(PAL.bg_deep, PAL.accent, 0.10f);
    for (int x = 0; x < GAME_W; x += 30)
        r_rect((float)x, FIELD_TOP, 1.0f, FIELD_BOTTOM - FIELD_TOP, c);
    for (int y = 16; y < (int)FIELD_BOTTOM; y += 30)
        r_rect(0.0f, (float)y, (float)GAME_W, 1.0f, c);
}

static void dashed_line(float x0, float y0, float x1, float y1,
                        float dash, float gap, float t, uint32_t c) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.01f) return;
    float ux = dx / len, uy = dy / len;
    float p = 0.0f;
    while (p < len) {
        float q = p + dash;
        if (q > len) q = len;
        r_line(x0 + ux * p, y0 + uy * p, x0 + ux * q, y0 + uy * q, t, c);
        p += dash + gap;
    }
}

static void draw_path(void) {
    if (g.path_n < 2) return;
    uint32_t under = mix_color(PAL.bg_deep, PAL.accent, 0.15f);
    uint32_t main = mix_color(PAL.bg_deep, PAL.accent, path_locked() ? 0.62f : 0.46f);

    unsigned char joint[PATH_MAX_POINTS];
    for (int i = 0; i < g.path_n; i++) {
        joint[i] = (i == 0 || i + 1 >= g.path_n);
        if (!joint[i]) {
            float ax = g.path[i].x - g.path[i - 1].x;
            float ay = g.path[i].y - g.path[i - 1].y;
            float bx = g.path[i + 1].x - g.path[i].x;
            float by = g.path[i + 1].y - g.path[i].y;
            float la = ax * ax + ay * ay;
            float lb = bx * bx + by * by;
            float cr = ax * by - ay * bx;
            if (cr * cr > 0.02f * la * lb) joint[i] = 1;
        }
    }

    for (int i = 0; i + 1 < g.path_n; i++) {
        r_line(g.path[i].x, g.path[i].y, g.path[i + 1].x, g.path[i + 1].y,
               19.0f, under);
    }
    for (int i = 0; i < g.path_n; i++)
        if (joint[i]) r_circle(g.path[i].x, g.path[i].y, 9.5f, under);

    for (int i = 0; i + 1 < g.path_n; i++) {
        r_line(g.path[i].x, g.path[i].y, g.path[i + 1].x, g.path[i + 1].y,
               14.0f, main);
    }
    for (int i = 0; i < g.path_n; i++)
        if (joint[i]) r_circle(g.path[i].x, g.path[i].y, 7.0f, main);

    for (int i = 0; i + 1 < g.path_n; i++)
        dashed_line(g.path[i].x, g.path[i].y, g.path[i + 1].x, g.path[i + 1].y,
                    5.0f, 6.0f, 1.0f, PAL.accent);

    /* start marker */
    Vec2 s = g.path[0];
    r_circle(s.x, s.y, 7.0f, PAL.bg_deep);
    r_ring(s.x, s.y, 7.0f, 1.5f, PAL.accent);
    r_text_center(s.x, s.y - 2.0f, "IN", PAL.accent, 1);
    r_text_center(s.x, s.y - 16.0f, "START", PAL.accent_dim, 1);
}

static void draw_castle(void) {
    if (!g.has_castle) return;
    float x = g.castle.x, y = g.castle.y;

    r_circle(x, y + 3.0f, CASTLE_RADIUS, C_RGBA(0, 0, 0, 130));
    r_circle(x, y, CASTLE_RADIUS, PAL.bg_panel);
    r_ring(x, y, CASTLE_RADIUS, 2.5f, PAL.accent);

    for (int i = 0; i < 8; i++) {
        float a = (float)i / 8.0f * 6.2831853f;
        float ca = cosf(a), sa = sinf(a);
        r_line(x + ca * (CASTLE_RADIUS - 6.0f), y + sa * (CASTLE_RADIUS - 6.0f),
               x + ca * (CASTLE_RADIUS + 3.0f), y + sa * (CASTLE_RADIUS + 3.0f),
               1.2f, PAL.accent);
    }
    r_line(x - 7.0f, y, x + 7.0f, y, 1.5f, PAL.accent);
    r_line(x, y - 7.0f, x, y + 7.0f, 1.5f, PAL.accent);
    r_line(x, y - CASTLE_RADIUS, x, y - CASTLE_RADIUS - 13.0f, 1.5f, PAL.accent_bright);
    {
        float p[6] = { x, y - CASTLE_RADIUS - 13.0f,
                       x + 10.0f, y - CASTLE_RADIUS - 9.5f,
                       x, y - CASTLE_RADIUS - 6.0f };
        r_poly(p, 3, PAL.accent);
    }

    float pct = g.castle_hp / g.castle_max_hp;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    if (pct > 0.0f) {
        uint32_t hc = pct > 0.5f ? PAL.accent : (pct > 0.25f ? PAL.accent_dark : PAL.danger);
        r_arc(x, y, CASTLE_RADIUS + 4.0f, -1.5707963f,
              -1.5707963f + 6.2831853f * pct, 3.0f, hc);
    }
}

static uint32_t tower_col(int type, int accent) {
    switch (type) {
        case T_ARROW:  return accent ? PAL.accent_bright : PAL.accent;
        case T_CANNON: return accent ? PAL.accent_bright : PAL.accent_dark;
        case T_FROST:  return accent ? PAL.accent_bright : PAL.accent_dim;
        case T_SNIPER: return accent ? PAL.accent_bright : PAL.accent_dim;
        case T_LASER:  return accent ? PAL.laser_glow : PAL.laser;
    }
    return accent ? PAL.accent_bright : PAL.accent;
}

static void draw_tower_shape(int type, float x, float y, float angle,
                             float scale, int level) {
    const TowerType *tt = &tower_types[type];
    float r = TOWER_RADIUS * scale;
    uint32_t col = tower_col(type, 0);
    uint32_t acc = tower_col(type, 1);

    r_circle(x, y + 2.0f * scale, r, C_RGBA(0, 0, 0, 120));
    r_circle(x, y, r, PAL.bg_panel);
    r_ring(x, y, r, 1.8f, col);
    r_ring(x, y, r * 0.65f, 1.0f, C_ALPHA(acc, 130));

    if (level > 0) {
        int teeth = level > MAX_LEVEL ? MAX_LEVEL : level;
        float outer = r - 1.0f;
        float inner = r * 0.35f;
        float half = 0.42f - (float)teeth * 0.04f;
        if (half < 0.18f) half = 0.18f;
        for (int i = 0; i < teeth; i++) {
            float a = -1.5707963f + (float)i / teeth * 6.2831853f;
            float al = a - half, ar = a + half;
            float p[6] = {
                x + cosf(al) * outer, y + sinf(al) * outer,
                x + cosf(a) * inner, y + sinf(a) * inner,
                x + cosf(ar) * outer, y + sinf(ar) * outer
            };
            r_poly(p, 3, C_ALPHA((i & 1) ? col : acc, 70));
            r_line(p[0], p[1], p[2], p[3], 1.2f, (i & 1) ? col : acc);
            r_line(p[2], p[3], p[4], p[5], 1.2f, (i & 1) ? col : acc);
        }
    }

    int barrel = tt->barrel;
    if (barrel == 4) {
        r_ring(x, y, r * 0.45f, 1.8f, PAL.laser);
        r_circle(x, y, r * 0.25f, PAL.laser);
        r_circle(x, y, r * 0.12f, PAL.laser_core);
        r_rot_rect(x, y, angle, r * 0.5f, -1.5f * scale,
                   r * 1.6f, 1.5f * scale, PAL.laser);
        r_rot_rect(x, y, angle, r * 1.45f, -2.0f * scale,
                   r * 1.45f + 5.0f * scale, 2.0f * scale, PAL.laser_glow);
    } else if (barrel == 1) {
        r_rot_rect(x, y, angle, 0.0f, -5.0f * scale,
                   r * 0.9f, 5.0f * scale, acc);
        r_rot_rect(x, y, angle, r * 0.8f, -6.0f * scale,
                   r * 0.8f + 9.0f * scale, 6.0f * scale, col);
        r_circle(x + cosf(angle) * (r * 0.8f + 9.0f * scale),
                 y + sinf(angle) * (r * 0.8f + 9.0f * scale),
                 3.5f * scale, PAL.bg_deep);
    } else if (barrel == 2) {
        r_rot_rect(x, y, angle, 0.0f, -6.0f * scale,
                   r * 1.0f, -2.0f * scale, acc);
        r_rot_rect(x, y, angle, 0.0f, 2.0f * scale,
                   r * 1.0f, 6.0f * scale, acc);
        r_rot_rect(x, y, angle, r * 0.9f, -7.0f * scale,
                   r * 0.9f + 7.0f * scale, -2.0f * scale, col);
        r_rot_rect(x, y, angle, r * 0.9f, 2.0f * scale,
                   r * 0.9f + 7.0f * scale, 7.0f * scale, col);
        r_circle(x + cosf(angle) * r * 0.6f,
                 y + sinf(angle) * r * 0.6f, 2.5f * scale, PAL.frost_proj);
    } else if (barrel == 3) {
        r_rot_rect(x, y, angle, 0.0f, -2.5f * scale,
                   r * 1.9f, 2.5f * scale, acc);
        r_rot_rect(x, y, angle, r * 1.7f, -3.5f * scale,
                   r * 1.7f + 7.0f * scale, 3.5f * scale, col);
        r_ring(x + cosf(angle) * r, y + sinf(angle) * r,
               3.5f * scale, 1.0f, acc);
    } else {
        r_rot_rect(x, y, angle, 0.0f, -3.0f * scale,
                   r * 1.2f, 3.0f * scale, acc);
        r_rot_rect(x, y, angle, r * 1.0f, -2.5f * scale,
                   r * 1.0f + 7.0f * scale, 2.5f * scale, col);
    }

    r_circle(x, y, 3.0f * scale, acc);
}

static void draw_range(float x, float y, float range, uint32_t col) {
    r_circle(x, y, range, C_ALPHA(col, 18));
    r_dashed_circle_phase(x, y, range, 1.2f, C_ALPHA(col, 130),
                          g.time_ms * 0.0012f);
}

static void draw_towers(void) {
    if (g.selected_tower >= 0 && g.selected_tower < g.tower_n) {
        Tower *t = &g.towers[g.selected_tower];
        TowerStats st = tower_stats(t);
        draw_range(t->x, t->y, (float)st.range, PAL.accent_bright);
    }
    for (int i = 0; i < g.tower_n; i++) {
        Tower *t = &g.towers[i];
        draw_tower_shape(t->type, t->x, t->y, t->angle, 1.0f, t->level);
    }
}

static void draw_enemy_icon(Enemy *e) {
    if (e->is_boss) {
        r_star(e->x, e->y, 5.0f, 2.2f, 5, -1.5707963f, PAL.enemy);
    } else if (e->speed > 0.34f * 1.4f) {
        float a = 0.0f;
        if (e->target_index < g.path_n) {
            a = atan2f(g.path[e->target_index].y - e->y,
                       g.path[e->target_index].x - e->x);
        }
        float p[6];
        for (int i = 0; i < 3; i++) {
            float aa = a + (float)i * 2.0943951f + 0.5f;
            float rr = (i == 0) ? 4.0f : 2.2f;
            p[i * 2] = e->x + cosf(aa) * rr;
            p[i * 2 + 1] = e->y + sinf(aa) * rr;
        }
        r_poly(p, 3, PAL.enemy);
    } else {
        r_circle(e->x, e->y, 2.2f, PAL.enemy);
    }
}

static void draw_enemy(Enemy *e) {
    float r = e->radius;
    r_circle(e->x, e->y + 2.0f, r, C_RGBA(0, 0, 0, 120));

    float p[8] = {
        e->x, e->y - r,
        e->x + r, e->y,
        e->x, e->y + r,
        e->x - r, e->y
    };
    r_poly(p, 4, PAL.bg_panel);
    r_line(p[0], p[1], p[2], p[3], 1.8f, PAL.enemy);
    r_line(p[2], p[3], p[4], p[5], 1.8f, PAL.enemy);
    r_line(p[4], p[5], p[6], p[7], 1.8f, PAL.enemy);
    r_line(p[6], p[7], p[0], p[1], 1.8f, PAL.enemy);

    draw_enemy_icon(e);

    if (e->slow_timer > 0) {
        r_dashed_circle(e->x, e->y, r + 3.0f, 1.0f, PAL.frost_proj);
    }

    float pct = e->hp / e->max_hp;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    float bx = e->x - 9.0f, by = e->y - r - 7.0f;
    r_rect(bx, by, 18.0f, 2.5f, PAL.bg_panel);
    uint32_t hc = pct > 0.5f ? PAL.accent : (pct > 0.25f ? PAL.accent_dark : PAL.danger);
    r_rect(bx, by, 18.0f * pct, 2.5f, hc);
    r_outline(bx, by, 18.0f, 2.5f, 0.6f, C_ALPHA(PAL.accent, 120));
}

static void draw_effects(void) {
    for (int i = 0; i < g.proj_n; i++) {
        Projectile *p = &g.projs[i];
        r_circle(p->x - p->vx * 0.8f, p->y - p->vy * 0.8f, 1.5f,
                 C_ALPHA(p->color, 120));
        r_circle(p->x, p->y, 2.5f, p->color);
    }

    for (int i = 0; i < g.beam_n; i++) {
        Beam *b = &g.beams[i];
        float a = (float)b->life / (float)b->max_life;
        r_line(b->x1, b->y1, b->x2, b->y2, b->width + a * 3.0f,
               C_ALPHA(b->color, (int)(a * 255.0f)));
        r_line(b->x1, b->y1, b->x2, b->y2,
               b->width * 0.4f < 1.0f ? 1.0f : b->width * 0.4f,
               C_ALPHA(PAL.laser_core, (int)(a * 255.0f)));
    }

    for (int i = 0; i < g.particle_n; i++) {
        Particle *p = &g.particles[i];
        float a = (float)p->life / 20.0f;
        if (a > 1.0f) a = 1.0f;
        r_circle(p->x, p->y, p->size, C_ALPHA(p->color, (int)(a * 255.0f)));
    }

    for (int i = 0; i < g.flash_n; i++) {
        UpgradeFlash *f = &g.flashes[i];
        float pct = (float)f->life / (float)f->max_life;
        r_ring(f->x, f->y, 14.0f + (1.0f - pct) * 18.0f, 2.0f,
               C_ALPHA(PAL.accent_bright, (int)(pct * 255.0f)));
        if (pct > 0.5f) {
            char b[8];
            snprintf(b, sizeof(b), "+1");
            r_text_center(f->x, f->y - 22.0f - (1.0f - pct) * 12.0f, b,
                          C_ALPHA(PAL.accent_bright, (int)(pct * 255.0f)), 1);
        }
    }
}

static void draw_build_preview(void) {
    if (g.mode != MODE_BUILD || g.pending_perk || g.game_over) return;
    int type = g.build_type;
    const TowerType *tt = &tower_types[type];
    int valid = can_place(g.cursor_x, g.cursor_y) && g.gold >= (float)tower_cost(type);
    float ang = angle_to_nearest_path(g.cursor_x, g.cursor_y);

    r_circle(g.cursor_x, g.cursor_y, tt->range,
             valid ? C_ALPHA(PAL.accent, 16) : C_ALPHA(PAL.danger, 16));
    r_dashed_circle_phase(g.cursor_x, g.cursor_y, tt->range, 1.2f,
                          valid ? C_ALPHA(PAL.accent, 120) : C_ALPHA(PAL.danger, 140),
                          g.time_ms * 0.0012f);

    draw_tower_shape(type, g.cursor_x, g.cursor_y, ang, 1.0f, 0);
    r_dashed_circle(g.cursor_x, g.cursor_y, TOWER_RADIUS,
                    1.2f, valid ? C_ALPHA(PAL.accent_bright, 180) : C_ALPHA(PAL.danger, 220));

    if (!valid) {
        r_line(g.cursor_x - 10.0f, g.cursor_y - 10.0f,
               g.cursor_x + 10.0f, g.cursor_y + 10.0f, 2.0f, PAL.danger);
        r_line(g.cursor_x + 10.0f, g.cursor_y - 10.0f,
               g.cursor_x - 10.0f, g.cursor_y + 10.0f, 2.0f, PAL.danger);
    }

    char b[32];
    snprintf(b, sizeof(b), "%s $%d", tt->name, tower_cost(type));
    r_text_center(g.cursor_x, g.cursor_y - 22.0f, b,
                  valid ? PAL.accent_bright : PAL.danger, 1);
}

static void draw_cursor(void) {
    float x = g.cursor_x, y = g.cursor_y;
    uint32_t c = PAL.accent_bright;
    r_line(x - 8.0f, y, x - 3.0f, y, 1.5f, c);
    r_line(x + 3.0f, y, x + 8.0f, y, 1.5f, c);
    r_line(x, y - 8.0f, x, y - 3.0f, 1.5f, c);
    r_line(x, y + 3.0f, x, y + 8.0f, 1.5f, c);
    r_circle(x, y, 1.0f, c);

    if (g.mode == MODE_DRAW && g.path_n > 0 && !path_locked()) {
        Vec2 last = g.path[g.path_n - 1];
        dashed_line(last.x, last.y, x, y, 4.0f, 5.0f, 1.0f,
                    C_ALPHA(PAL.accent, 90));
        if (g.drawing) {
            float pulse = 4.0f + sinf((float)g.frame * 0.25f) * 2.0f;
            r_ring(last.x, last.y, pulse, 1.5f, PAL.accent_bright);
        }
    }
}

static void draw_topbar(void) {
    r_rect(0, 0, 480, 15, PAL.bg_panel);
    r_rect(0, 14, 480, 1, PAL.accent);

    char b[32];
    r_text(4, 2, "G", PAL.accent_dim, 1);
    format_amount(g.gold, b, sizeof(b));
    r_text(18, 2, b, PAL.accent_bright, 1);

    r_text(60, 2, "HP", PAL.accent_dim, 1);
    snprintf(b, sizeof(b), "%d", (int)g.castle_hp);
    r_text(81, 2, b, g.castle_hp <= g.castle_max_hp * 0.3f ? PAL.danger : PAL.accent_bright, 1);

    r_text(124, 2, "W", PAL.accent_dim, 1);
    snprintf(b, sizeof(b), "%d", g.wave);
    r_text(138, 2, b, PAL.accent_bright, 1);

    r_text(174, 2, "E", PAL.accent_dim, 1);
    snprintf(b, sizeof(b), "%d", g.wave_active ? g.enemy_n + g.enemies_to_spawn : 0);
    r_text(188, 2, b, PAL.accent_bright, 1);

    r_text(228, 2, "T", PAL.accent_dim, 1);
    snprintf(b, sizeof(b), "%d", g.tower_n);
    r_text(242, 2, b, PAL.accent_bright, 1);

    const char *mode = "PATH";
    if (g.paused) mode = "PAUSE";
    else if (g.game_over) mode = "OVER";
    else if (g.pending_perk) mode = "PERK";
    else if (g.mode == MODE_BUILD) mode = "BUILD";
    else if (g.mode == MODE_SELECT) mode = "TOWER";
    else if (path_locked()) mode = "FREE";
    char mb[32];
    if (g.wave_active && !g.pending_perk)
        snprintf(mb, sizeof(mb), "%s *", mode);
    else
        snprintf(mb, sizeof(mb), "%s", mode);
    r_text_right(476, 2, mb, PAL.accent, 1);
}

static void draw_bottombar(void) {
    r_rect(0, 250, 480, 22, PAL.bg_panel);
    r_rect(0, 250, 480, 1, PAL.accent);

    for (int i = 0; i < T_COUNT; i++) {
        float x = 2.0f + (float)i * 47.0f;
        float y = 252.0f;
        int cost = tower_cost(i);
        int selected = (g.mode == MODE_BUILD && g.build_type == i);
        uint32_t bg = selected ? PAL.bg_hover : PAL.bg_deep;
        uint32_t br = selected ? PAL.accent_bright :
                      (g.gold >= (float)cost ? PAL.accent_dim : PAL.accent_deep);
        r_rect(x, y, 45, 18, bg);
        r_outline(x, y, 45, 18, selected ? 1.5f : 1.0f, br);
        draw_tower_shape(i, x + 9.0f, y + 9.0f, -1.5707963f, 0.48f, 0);
        char cb[16];
        snprintf(cb, sizeof(cb), "$%d", cost);
        r_text(x + 17.0f, y + 5.0f, cb,
               g.gold >= (float)cost ? PAL.accent : PAL.accent_deep, 1);
    }

    float x = 242.0f;
    char b[96];
    if (g.game_over) {
        r_text(x, 252, "GAME OVER", PAL.danger, 1);
        r_text(x, 262, ICON_CROSS ": RESTART", PAL.accent, 1);
    } else if (g.mode == MODE_BUILD) {
        TowerStats st = tower_stats(&(Tower){ .type = g.build_type, .level = 0 });
        snprintf(b, sizeof(b), "%s $%d  DMG %d  RNG %d",
                 tower_types[g.build_type].name, tower_cost(g.build_type),
                 st.damage, st.range);
        r_text(x, 252, b, PAL.accent, 1);
        r_text(x, 262, ICON_CROSS ": PLACE  L/R: TYPE  " ICON_TRIANGLE ": UPGRADE", PAL.accent_dim, 1);
    } else if (g.mode == MODE_SELECT) {
        r_text(x, 252, "TOWER SELECTED", PAL.accent_bright, 1);
        r_text(x, 262, ICON_TRIANGLE ": UPGRADE  " ICON_SQUARE ": SELL", PAL.accent_dim, 1);
    } else if (g.path_n < 2) {
        r_text(x, 252, "DRAW PATH: HOLD " ICON_CROSS, PAL.accent, 1);
        r_text(x, 262, ICON_SQUARE ": BUILD  START: WAVE", PAL.accent_dim, 1);
    } else if (g.mode == MODE_DRAW && path_locked()) {
        r_text(x, 252, "FREE CURSOR  " ICON_SQUARE ": BUILD", PAL.accent, 1);
        r_text(x, 262, "START: WAVE  SELECT: PAUSE", PAL.accent_dim, 1);
    } else {
        r_text(x, 252, "PATH READY  -  " ICON_SQUARE ": BUILD", PAL.accent, 1);
        r_text(x, 262, ICON_CIRCLE ": DELETE  START: WAVE", PAL.accent_dim, 1);
    }
}

static void draw_tower_panel(void) {
    if (g.selected_tower < 0 || g.selected_tower >= g.tower_n) return;
    Tower *t = &g.towers[g.selected_tower];
    TowerStats st = tower_stats(t);
    const char *nm = tower_types[t->type].name;

    float x = 296.0f, y = 16.0f, w = 178.0f, h = 84.0f;
    r_rect(x, y, w, h, C_ALPHA(PAL.bg_panel, 244));
    r_outline(x, y, w, h, 1.0f, PAL.accent);
    r_rect(x + 2, y + 2, w - 4, 1, C_ALPHA(PAL.accent, 120));

    char b[64];
    snprintf(b, sizeof(b), "%s  LV %d/%d", nm, t->level, MAX_LEVEL);
    r_text(x + 5, y + 4, b, PAL.accent_bright, 1);

    int maxed = t->level >= MAX_LEVEL;
    if (!maxed) {
        int nx = (int)(tower_types[t->type].damage * (1.0f + 0.30f * (t->level + 1)) + 0.5f);
        snprintf(b, sizeof(b), "DMG %d > %d", st.damage, nx);
        r_text(x + 5, y + 16, b, PAL.accent, 1);
        nx = (int)(tower_types[t->type].range * (1.0f + 0.10f * (t->level + 1)) + 0.5f);
        snprintf(b, sizeof(b), "RNG %d > %d", st.range, nx);
        r_text(x + 5, y + 26, b, PAL.accent, 1);
        int nr = (int)(tower_types[t->type].fire_rate * (1.0f - 0.10f * (t->level + 1)) + 0.5f);
        if (nr < 6) nr = 6;
        snprintf(b, sizeof(b), "RATE %.1f/s > %.1f", 60.0f / (float)st.fire_rate,
                 60.0f / (float)nr);
        r_text(x + 5, y + 36, b, PAL.accent, 1);
    } else {
        snprintf(b, sizeof(b), "DMG %d", st.damage);
        r_text(x + 5, y + 16, b, PAL.accent, 1);
        snprintf(b, sizeof(b), "RNG %d", st.range);
        r_text(x + 5, y + 26, b, PAL.accent, 1);
        snprintf(b, sizeof(b), "RATE %.1f/s", 60.0f / (float)st.fire_rate);
        r_text(x + 5, y + 36, b, PAL.accent, 1);
    }

    if (!maxed) {
        int uc = upgrade_cost(t);
        snprintf(b, sizeof(b), ICON_TRIANGLE ": UPGRADE $%d", uc);
        r_text(x + 5, y + 50, b, g.gold >= (float)uc ? PAL.accent_bright : PAL.danger, 1);
    } else {
        r_text(x + 5, y + 50, ICON_TRIANGLE ": MAX LEVEL", PAL.accent_dim, 1);
    }

    int rf = sell_refund(t);
    snprintf(b, sizeof(b), "HOLD " ICON_SQUARE ": SELL +$%d", rf);
    r_text(x + 5, y + 61, b, PAL.danger_soft, 1);
    r_text(x + 5, y + 72, ICON_CIRCLE ": CLOSE", PAL.accent_dim, 1);

    if (g.sell_hold > 0) {
        float pct = (float)g.sell_hold / 30.0f;
        if (pct > 1.0f) pct = 1.0f;
        r_rect(x + 3, y + h - 3, (w - 6) * pct, 2, PAL.danger);
    }
}

static void draw_toast(void) {
    if (g.toast_timer <= 0 || g.toast[0] == 0) return;
    float a = 1.0f;
    if (g.toast_timer < 20) a = (float)g.toast_timer / 20.0f;
    float w = r_text_width(g.toast, 1) + 16.0f;
    if (w > 470.0f) w = 470.0f;
    float x = (480.0f - w) * 0.5f;
    r_rect(x, 17.0f, w, 15.0f, C_ALPHA(PAL.bg_panel, (int)(220.0f * a)));
    r_outline(x, 17.0f, w, 15.0f, 1.0f, C_ALPHA(PAL.accent, (int)(200.0f * a)));
    r_text_center(240.0f, 20.0f, g.toast, C_ALPHA(PAL.accent_bright, (int)(255.0f * a)), 1);
}

static void draw_wave_banner(void) {
    if (g.wave_banner <= 0) return;
    float a = 1.0f;
    if (g.wave_banner > 90) a = (float)(120 - g.wave_banner) / 30.0f;
    if (g.wave_banner < 30) a = (float)g.wave_banner / 30.0f;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    char b[32];
    snprintf(b, sizeof(b), "WAVE %d", g.wave);
    r_text_glow(240.0f - r_text_width(b, 3) * 0.5f, 62.0f, b,
                C_ALPHA(PAL.accent_bright, (int)(a * 255.0f)),
                C_ALPHA(PAL.accent, (int)(a * 180.0f)), 3);
}

static void draw_mod_popup(void) {
    if (g.mod_banner <= 0 || g.mod_n <= 0) return;
    int id = g.active_mods[g.mod_n - 1];
    if (id < 0 || id >= modifier_count) return;

    float a = 1.0f;
    if (g.mod_banner > 150) a = (float)(180 - g.mod_banner) / 30.0f;
    if (g.mod_banner < 30) a = (float)g.mod_banner / 30.0f;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;

    float x = 50.0f, y = 76.0f, w = 380.0f, h = 66.0f;
    r_rect(x, y, w, h, C_ALPHA(PAL.bg_deep, (int)(238.0f * a)));
    r_outline(x, y, w, h, 1.5f, C_ALPHA(PAL.danger, (int)(255.0f * a)));
    r_text_center(240.0f, y + 8.0f, "ENEMY MODIFIER",
                  C_ALPHA(PAL.danger, (int)(255.0f * a)), 1);
    r_text_center(240.0f, y + 24.0f, modifiers[id].name,
                  C_ALPHA(PAL.accent_bright, (int)(255.0f * a)), 2);
    r_text_center(240.0f, y + 46.0f, "EVERY 10 WAVES",
                  C_ALPHA(PAL.accent_dim, (int)(255.0f * a)), 1);
}

static uint32_t rarity_color(int r) {
    if (r == RARITY_CURSED) return PAL.danger;
    if (r == RARITY_LEGENDARY) return PAL.accent_bright;
    if (r == RARITY_RARE) return PAL.accent;
    return PAL.accent_dim;
}

static const char *rarity_label(int r) {
    if (r == RARITY_CURSED) return "CURSED";
    if (r == RARITY_LEGENDARY) return "LEGENDARY";
    if (r == RARITY_RARE) return "RARE";
    return "COMMON";
}

static void draw_perk_modal(void) {
    if (!g.pending_perk) return;
    r_rect(0, 0, 480, 272, C_ALPHA(PAL.bg_deep, 238));
    r_text_glow(240.0f - r_text_width("CHOOSE UPGRADE", 2) * 0.5f, 8.0f,
                "CHOOSE UPGRADE", PAL.accent_bright, PAL.accent, 2);
    char sub[64];
    snprintf(sub, sizeof(sub), "WAVE %d CLEARED - PICK 1 OF 3", g.wave);
    r_text_center(240.0f, 30.0f, sub, PAL.accent, 1);

    for (int i = 0; i < 3; i++) {
        int id = g.perk_choices[i];
        const Perk *p = &perks[id];
        float x = 8.0f + (float)i * 158.0f;
        float y = 44.0f, w = 148.0f, h = 180.0f;
        int sel = (i == g.perk_sel);
        uint32_t rc = rarity_color(p->rarity);
        r_rect(x, y, w, h, PAL.bg_panel);
        r_outline(x, y, w, h, sel ? 2.0f : 1.0f, sel ? PAL.accent_bright : rc);
        if (sel) r_rect(x + 2, y + 2, w - 4, 2, PAL.accent_bright);

        r_text(x + 5, y + 5, rarity_label(p->rarity), rc, 1);
        r_text(x + 5, y + 19, p->name, PAL.accent_bright, 1);
        if (g.perk_stacks[id] > 0) {
            char st[16];
            snprintf(st, sizeof(st), "X%d", g.perk_stacks[id] + 1);
            r_text_right(x + w - 5, y + 5, st, PAL.accent_bright, 1);
        }
        r_text_wrap(x + 5, y + 34, w - 10, 10.0f, p->desc, PAL.accent, 1);
        if (p->rarity == RARITY_CURSED && p->cursed) {
            r_text(x + 5, y + h - 36, "! CURSE:", PAL.danger, 1);
            r_text_wrap(x + 5, y + h - 25, w - 10, 9.0f, p->cursed, PAL.danger_soft, 1);
        }
        if (sel) {
            r_text(x + 4, y + h - 13, ">", PAL.accent_bright, 1);
        }
    }

    int cost = reroll_cost();
    char foot[96];
    if (cost == 0) snprintf(foot, sizeof(foot), "L/R: PICK   " ICON_CROSS ": TAKE   " ICON_SQUARE ": REROLL FREE");
    else snprintf(foot, sizeof(foot), "L/R: PICK   " ICON_CROSS ": TAKE   " ICON_SQUARE ": REROLL $%d", cost);
    r_text_center(240.0f, 232.0f, foot, cost > 0 && g.gold < cost ? PAL.danger : PAL.accent, 1);
    r_text_center(240.0f, 244.0f, "CURSES GRANT POWER BUT STRENGTHEN ENEMIES", PAL.accent_dim, 1);
}

static void active_row(int row, const char *text, const char *right, uint32_t color) {
    int vis = row - active_scroll;
    if (vis < 0 || vis >= ACTIVE_ROWS) return;
    float y = 70.0f + (float)vis * 11.0f;
    r_text(14.0f, y, text, color, 1);
    if (right && right[0]) r_text_right(466.0f, y, right, color, 1);
}

static void draw_active_page(void) {
    int nperks = active_perk_count();
    int nmods = g.mod_n;
    int cursed = active_cursed_count();
    int total = active_line_count();
    int max_scroll = total - ACTIVE_ROWS;
    if (max_scroll < 0) max_scroll = 0;
    if (active_scroll > max_scroll) active_scroll = max_scroll;

    r_text_glow(240.0f - r_text_width("ACTIVE PERKS & MODS", 2) * 0.5f, 38.0f,
                "ACTIVE PERKS & MODS", PAL.accent_bright, PAL.accent, 2);
    char count[64];
    snprintf(count, sizeof(count), "PERKS %d   MODS %d", nperks, nmods);
    r_text_center(240.0f, 58.0f, count, PAL.accent_dim, 1);

    if (total == 0) {
        r_text_center(240.0f, 124.0f, "NO ACTIVE PERKS OR MODIFIERS", PAL.accent_dim, 1);
    } else {
        int row = 0;
        if (nperks > 0) {
            char h[32];
            snprintf(h, sizeof(h), "PERKS (%d)", nperks);
            active_row(row++, h, NULL, PAL.accent_bright);
            for (int i = 0; i < P_COUNT; i++) {
                if (!g.perk_stacks[i]) continue;
                char label[64], cnt[16];
                int is_cursed = perks[i].rarity == RARITY_CURSED;
                snprintf(label, sizeof(label), "%s%s", is_cursed ? "! " : "", perks[i].name);
                snprintf(cnt, sizeof(cnt), "x%d", g.perk_stacks[i]);
                active_row(row++, label, cnt, is_cursed ? PAL.danger : PAL.accent);
            }
        }
        if (nmods > 0) {
            char h[32];
            snprintf(h, sizeof(h), "MODIFIERS (%d)", nmods);
            active_row(row++, h, NULL, PAL.danger);
            for (int i = 0; i < nmods; i++) {
                int id = g.active_mods[i];
                if (id < 0 || id >= modifier_count) continue;
                active_row(row++, modifiers[id].name, NULL, PAL.danger_soft);
            }
        }
    }

    if (cursed > 0)
        r_text_center(240.0f, 234.0f, "! = CURSED PERK", PAL.danger, 1);
    r_text_center(240.0f, 248.0f, "UP/DOWN: SCROLL   " ICON_CIRCLE "/" ICON_CROSS ": BACK",
                  PAL.accent, 1);
    r_text(6.0f, 260.0f, "MADE BY DUDE7OX", PAL.accent_dim, 1);
}

static void draw_pause(void) {
    if (!g.paused) return;
    r_rect(0, 0, 480, 272, C_ALPHA(PAL.bg_deep, 228));
    r_text_glow(240.0f - r_text_width("PAUSE", 3) * 0.5f, 6.0f, "PAUSE",
                PAL.accent_bright, PAL.accent, 3);
    {
        char bv[16];
        snprintf(bv, sizeof(bv), "V%s", GAME_VERSION);
        r_text_right(474.0f, 20.0f, bv, PAL.accent_dim, 1);
    }

    if (g.pause_help == 1) {
        const char *lines[] = {
            "ANALOG / D-PAD   CURSOR",
            ICON_CROSS "   DRAW / PLACE / SELECT",
            ICON_CIRCLE "   CANCEL / DELETE POINT",
            ICON_SQUARE "  DRAW / BUILD",
            "L/R  SELECT TOWER TYPE",
            ICON_TRIANGLE "  UPGRADE TOWER",
            "HOLD " ICON_SQUARE "  SELL (IN PANEL)",
            "START  START WAVE",
            "SELECT  PAUSE",
            "LEFT/RIGHT  CURSOR SPEED",
            "PERKS & MODS  IN PAUSE",
            "EVERY 5 WAVES - PERK",
            "EVERY 10 WAVES - MODIFIER",
        };
        for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++)
            r_text(36.0f, 62.0f + (float)i * 13.0f, lines[i],
                   (i & 1) ? PAL.accent_dim : PAL.accent, 1);
        r_text_center(240.0f, 248.0f, ICON_CIRCLE " / " ICON_CROSS ": BACK", PAL.accent_bright, 1);
        r_text(6.0f, 260.0f, "MADE BY DUDE7OX", PAL.accent_dim, 1);
        return;
    }
    if (g.pause_help == 2) {
        draw_active_page();
        return;
    }

    char b[128];
    snprintf(b, sizeof(b), "GOLD %d   CASTLE %d   WAVE %d   TOWERS %d",
             (int)g.gold, (int)g.castle_hp, g.wave, g.tower_n);
    r_text_center(240.0f, 34.0f, b, PAL.accent, 1);

    const char *items[9];
    char theme[32], sound[32], speed[32];
    snprintf(theme, sizeof(theme), "THEME: %s", g.theme == 0 ? "AMBER" : (g.theme == 1 ? "BLUE" : "GREEN"));
    snprintf(sound, sizeof(sound), "SOUND: %s", g.sound_muted ? "OFF" : "ON");
    if (g.cursor_speed_pct <= 0)
        snprintf(speed, sizeof(speed), "CURSOR SPEED: 0.1%%");
    else
        snprintf(speed, sizeof(speed), "CURSOR SPEED: %d%%", g.cursor_speed_pct);
    items[0] = "RESUME";
    items[1] = "SAVE GAME";
    items[2] = "LOAD GAME";
    items[3] = "RESET GAME";
    items[4] = theme;
    items[5] = sound;
    items[6] = speed;
    items[7] = "PERKS & MODS";
    items[8] = "HELP";

    for (int i = 0; i < 9; i++) {
        uint32_t c = (i == g.pause_sel) ? PAL.accent_bright : PAL.accent_dim;
        if (i == g.pause_sel) r_text(120.0f, 44.0f + (float)i * 21.0f, ">", c, 2);
        r_text(140.0f, 44.0f + (float)i * 21.0f, items[i], c, 2);
    }

    int nperks = active_perk_count();
    int cursed = active_cursed_count();
    char left[64], right[32];
    snprintf(left, sizeof(left), "PERKS %d   MODS %d", nperks, g.mod_n);
    if (cursed > 0) snprintf(right, sizeof(right), "CURSED %d", cursed);
    else right[0] = 0;
    float lw = r_text_width(left, 1);
    float rw = right[0] ? r_text_width(right, 1) : 0.0f;
    float totalw = lw + (right[0] ? 12.0f + rw : 0.0f);
    float sx = 240.0f - totalw * 0.5f;
    r_text(sx, 236.0f, left, PAL.accent_dim, 1);
    if (right[0]) r_text(sx + lw + 12.0f, 236.0f, right, PAL.danger, 1);
    if (g.pause_sel == 6)
        r_text_center(240.0f, 250.0f, "LEFT/RIGHT: ADJUST   X: CYCLE", PAL.accent, 1);
    else
        r_text_center(240.0f, 250.0f, "START: RESUME   " ICON_CROSS ": SELECT", PAL.accent, 1);
    r_text(6.0f, 262.0f, "MADE BY DUDE7OX", PAL.accent_dim, 1);
}

static void draw_gameover(void) {
    if (!g.game_over) return;
    r_rect(0, 0, 480, 272, C_ALPHA(PAL.bg_deep, 235));
    r_text_glow(240.0f - r_text_width("GAME OVER", 4) * 0.5f, 62.0f, "GAME OVER",
                PAL.danger, PAL.danger, 4);
    char b[64];
    snprintf(b, sizeof(b), "WAVES SURVIVED: %d", g.wave);
    r_text_center(240.0f, 120.0f, b, PAL.accent, 2);
    snprintf(b, sizeof(b), "GOLD: %d   TOWERS: %d", (int)g.gold, g.tower_n);
    r_text_center(240.0f, 144.0f, b, PAL.accent_dim, 1);
    r_text_center(240.0f, 176.0f, ICON_CROSS " / START: RESTART", PAL.accent_bright, 2);
}

void game_draw(void) {
    r_rect(0, 0, 480, 272, PAL.bg_deep);
    draw_grid();
    draw_path();
    draw_castle();
    draw_towers();
    draw_effects();
    for (int i = 0; i < g.enemy_n; i++) draw_enemy(&g.enemies[i]);
    draw_build_preview();
    draw_cursor();
    draw_topbar();
    draw_bottombar();
    draw_tower_panel();
    draw_wave_banner();
    draw_mod_popup();
    draw_perk_modal();
    draw_pause();
    draw_gameover();
    draw_toast();
}

/* ------------------------------------------------------------------ */
/* lifecycle                                                           */
/* ------------------------------------------------------------------ */

void game_reset(void) {
    int theme = g.theme;
    int muted = g.sound_muted;
    int speed = g.cursor_speed_pct;
    if (speed < 0 || speed > 100) speed = 100;

    memset(&g, 0, sizeof(g));
    g.theme = theme;
    g.sound_muted = muted;
    g.cursor_speed_pct = speed;
    PAL = palettes[theme];
    g.gold = 200.0f;
    g.castle_hp = g.castle_max_hp = 100.0f;
    g.wave = 0;
    g.mode = MODE_DRAW;
    g.path_n = 0;
    g.selected_tower = -1;
    g.build_type = T_ARROW;
    g.build_rotation = -1.5707963f;
    g.cursor_x = 240.0f;
    g.cursor_y = 140.0f;
    g.mod_enemy_hp = 1.0f;
    g.mod_enemy_speed = 1.0f;
    g.mod_boss_chance = 0.0f;
    g.mod_count = 1.0f;
    g.mod_reward = 1.0f;
    g.mod_boss_hp = 1.0f;
    g.paused = 0;
    g.pause_sel = 0;
    g.pause_help = 0;
    for (int i = 0; i < 32; i++) g.last_sfx[i] = -10000.0f;
}

void game_after_load(void) {
    if (g.theme < 0 || g.theme > 2) g.theme = 0;
    theme_apply(g.theme);
    audio_set_muted(g.sound_muted);
    if (g.cursor_speed_pct < 0) g.cursor_speed_pct = 0;
    if (g.cursor_speed_pct > 100) g.cursor_speed_pct = 100;

    if (g.path_n < 0) g.path_n = 0;
    if (g.path_n > PATH_MAX_POINTS) g.path_n = PATH_MAX_POINTS;
    if (g.tower_n < 0) g.tower_n = 0;
    if (g.tower_n > TOWER_MAX) g.tower_n = TOWER_MAX;
    if (g.enemy_n < 0) g.enemy_n = 0;
    if (g.enemy_n > ENEMY_MAX) g.enemy_n = ENEMY_MAX;
    if (g.proj_n < 0) g.proj_n = 0;
    if (g.proj_n > PROJ_MAX) g.proj_n = PROJ_MAX;
    if (g.particle_n < 0) g.particle_n = 0;
    if (g.particle_n > PARTICLE_MAX) g.particle_n = PARTICLE_MAX;
    if (g.beam_n < 0) g.beam_n = 0;
    if (g.beam_n > BEAM_MAX) g.beam_n = BEAM_MAX;
    if (g.flash_n < 0) g.flash_n = 0;
    if (g.flash_n > FLASH_MAX) g.flash_n = FLASH_MAX;
    if (g.mod_n < 0) g.mod_n = 0;
    if (g.mod_n > MOD_MAX) g.mod_n = MOD_MAX;
    for (int i = 0; i < g.mod_n; i++) {
        if (g.active_mods[i] < 0 || g.active_mods[i] >= modifier_count)
            g.active_mods[i] = 0;
    }
    if (g.build_type < 0 || g.build_type >= T_COUNT) g.build_type = T_ARROW;
    if (g.perk_sel < 0 || g.perk_sel > 2) g.perk_sel = 0;
    if (g.selected_tower < 0 || g.selected_tower >= g.tower_n)
        g.selected_tower = -1;
    if (g.wave < 0) g.wave = 0;
    if (g.castle_max_hp < 1.0f) g.castle_max_hp = 100.0f;
    if (g.castle_hp < 0.0f) g.castle_hp = 0.0f;
    if (g.castle_hp > g.castle_max_hp) g.castle_hp = g.castle_max_hp;
    g.cursor_x = clampf(g.cursor_x, FIELD_LEFT, FIELD_RIGHT);
    g.cursor_y = clampf(g.cursor_y, FIELD_TOP + 2.0f, FIELD_BOTTOM - 2.0f);
    g.mode = (g.selected_tower >= 0) ? MODE_SELECT : MODE_DRAW;
    g.drawing = 0;
    g.paused = 0;
    g.pause_sel = 0;
    g.pause_help = 0;
    g.sell_hold = 0;
    g.wave_banner = 0;
    g.mod_banner = 0;
    g.toast_timer = 0;
    g.toast[0] = 0;
    for (int i = 0; i < 32; i++) g.last_sfx[i] = -10000.0f;
    update_castle();
    toast_set("GAME LOADED");
}

void game_init(void) {
    memset(&g, 0, sizeof(g));
    g.theme = 0;
    g.sound_muted = 0;
    g.cursor_speed_pct = 100;
    PAL = palettes[0];
    game_reset();
    rng_state = 0x9e3779b9u;
}
