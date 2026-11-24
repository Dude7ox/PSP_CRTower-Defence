#ifndef CRTD_GAME_H
#define CRTD_GAME_H

#include <stdint.h>

#define GAME_TITLE   "CRTower Defence"
#define GAME_VERSION "0.5"

#define GAME_W 480
#define GAME_H 272
#define FIELD_TOP 15.0f
#define FIELD_BOTTOM 249.0f
#define FIELD_LEFT 3.0f
#define FIELD_RIGHT 477.0f

#define PATH_MAX_POINTS 256
#define ENEMY_MAX 200
#define TOWER_MAX 64
#define PROJ_MAX 256
#define PARTICLE_MAX 300
#define BEAM_MAX 60
#define FLASH_MAX 16
#define MOD_MAX 16

#define MAX_LEVEL 6
#define TOWER_RADIUS 10.0f
#define ENEMY_RADIUS 7.0f
#define CASTLE_RADIUS 15.0f

enum { T_ARROW, T_CANNON, T_FROST, T_SNIPER, T_LASER, T_COUNT };
enum { MODE_DRAW, MODE_BUILD, MODE_SELECT };
enum { RARITY_COMMON, RARITY_RARE, RARITY_LEGENDARY, RARITY_CURSED };

enum {
    P_GOLD_BONUS, P_CASTLE_HP, P_ARROW_DMG, P_ARROW_RATE, P_CANNON_DMG,
    P_CANNON_SPLASH, P_FROST_SLOW, P_FROST_DURATION, P_SNIPER_DMG,
    P_SNIPER_RATE, P_LASER_DMG, P_LASER_RATE, P_LASER_WIDTH, P_ALL_DMG,
    P_ALL_RANGE, P_DISCOUNT, P_UPGRADE_DISCOUNT, P_SELL_BONUS, P_INTEREST,
    P_DOUBLE_SHOT, P_GOLD_RUSH,
    P_CURSED_FURY, P_CURSED_SPEED, P_CURSED_GOLD, P_CURSED_RANGE,
    P_CURSED_CANNON, P_CURSED_LASER, P_CURSED_FROST, P_CURSED_ECONOMY,
    P_COUNT
};

typedef struct { float x, y; } Vec2;

typedef struct {
    const char *name;
    const char *short_name;
    int cost;
    float range;
    float damage;
    int fire_rate;
    float proj_speed;
    float splash;
    float slow;
    int slow_duration;
    int laser;
    int barrel;      /* 0 short, 1 thick, 2 dual, 3 long, 4 emitter */
} TowerType;

typedef struct {
    int type;
    float x, y, angle;
    int cooldown;
    int level;
    int invested;
} Tower;

typedef struct {
    float x, y;
    float hp, max_hp;
    float speed;
    float radius;
    int target_index;
    float slow_factor;
    int slow_timer;
    float wobble;
    int reward;
    int is_boss;
} Enemy;

typedef struct {
    float x, y, vx, vy;
    int damage;
    int life;
    int type;
    float splash;
    float slow;
    int slow_duration;
    uint32_t color;
} Projectile;

typedef struct {
    float x, y, vx, vy;
    int life;
    int max_life;
    uint32_t color;
    float size;
} Particle;

typedef struct {
    float x1, y1, x2, y2;
    int life, max_life;
    uint32_t color, glow;
    float width;
} Beam;

typedef struct {
    float x, y;
    int life, max_life;
} UpgradeFlash;

typedef struct {
    int id;
    const char *name;
    const char *desc;
    const char *cursed;
    int rarity;
} Perk;

typedef struct {
    int id;
    const char *name;
} Modifier;

typedef struct {
    float gold;
    float castle_hp, castle_max_hp;
    int wave;
    int mode;

    Vec2 path[PATH_MAX_POINTS];
    int path_n;
    int drawing;
    Vec2 castle;
    int has_castle;

    Enemy enemies[ENEMY_MAX];
    int enemy_n;
    Tower towers[TOWER_MAX];
    int tower_n;
    Projectile projs[PROJ_MAX];
    int proj_n;
    Particle particles[PARTICLE_MAX];
    int particle_n;
    Beam beams[BEAM_MAX];
    int beam_n;
    UpgradeFlash flashes[FLASH_MAX];
    int flash_n;

    int wave_active;
    int enemies_to_spawn;
    int enemies_total;
    int spawn_timer;
    int game_over;
    int pending_perk;

    int perk_choices[3];
    int perk_sel;
    int reroll_count;
    int reroll_used;
    float perk[P_COUNT];
    int perk_stacks[P_COUNT];
    float enemy_hp_from_perk;

    int active_mods[MOD_MAX];
    int mod_n;
    float mod_enemy_hp;
    float mod_enemy_speed;
    float mod_boss_chance;
    float mod_count;
    float mod_reward;
    float mod_boss_hp;

    int last_perk_wave;
    int last_mod_wave;

    int build_type;
    int selected_tower;
    float build_rotation;

    int paused;
    int pause_sel;
    int pause_help;
    int sound_muted;
    int theme;

    float cursor_x, cursor_y;
    int sell_hold;

    char toast[96];
    int toast_timer;
    int wave_banner;
    int mod_banner;

    int frame;
    float time_ms;
    float last_sfx[32];

    /* Appended so old save files can be migrated by size. 0 = 0.1% speed,
       100 = the original cursor speed. */
    int cursor_speed_pct;
} Game;

extern Game g;
extern const TowerType tower_types[T_COUNT];
extern const Perk perks[P_COUNT];
extern const Modifier modifiers[];
extern const int modifier_count;

void game_init(void);
void game_reset(void);
void game_update(float dt, uint32_t buttons, uint32_t pressed, float mx, float my);
void game_draw(void);
void game_after_load(void);

#endif
