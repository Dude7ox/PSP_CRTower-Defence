#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>

#include "save.h"
#include "game.h"

#define SAVE_MAGIC       0x50535444u   /* "PSTD": kept for old saves */
#define SAVE_VERSION     3u
#define SAVE_MIN_VERSION 2u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t checksum;
} SaveHeader;

static Game load_game;
static char save_path[96];

static uint32_t hash_bytes(const void *data, size_t n) {
    const unsigned char *p = (const unsigned char *)data;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint32_t)p[i];
        h *= 16777619u;
    }
    return h;
}

static void toast(const char *msg) {
    snprintf(g.toast, sizeof(g.toast), "%s", msg);
    g.toast_timer = 130;
}

#define SAVE_DIR "ms0:/PSP/GAME/CRTowerDefence"

static int file_exists(const char *path) {
    SceIoStat st;
    return sceIoGetstat(path, &st) >= 0;
}

static int copy_file(const char *src, const char *dst) {
    SceUID in = sceIoOpen(src, PSP_O_RDONLY, 0);
    if (in < 0) return 0;
    SceUID out = sceIoOpen(dst, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (out < 0) {
        sceIoClose(in);
        return 0;
    }

    char buf[4096];
    int ok = 1;
    int n;
    while ((n = sceIoRead(in, buf, sizeof(buf))) > 0) {
        if (sceIoWrite(out, buf, n) != n) {
            ok = 0;
            break;
        }
    }
    if (n < 0) ok = 0;
    sceIoClose(in);
    sceIoClose(out);
    if (!ok) sceIoRemove(dst);
    return ok;
}

static void resolve_save_path(void) {
    /* New folder first, then the folders used by pre-release builds. */
    static const char *dirs[] = {
        SAVE_DIR,
        "ms0:/PSP/GAME/CRTower Defence",
        "ms0:/PSP/GAME/PSTD",
        "ms0:/PSP/GAME/PSTD00001",
        "ms0:/PSP/GAME/TOWERDEFENCE",
        "ms0:/PSP/GAME/TowerDefence",
        "ms0:/PSP/GAME/TowerDefense",
        "ms0:/PSP/GAME/PSTOWERDEF",
        "ms0:/PSP/GAME/TOWER-DEFENCE",
        "ms0:/PSP/GAME/Tower Defence"
    };

    const char *current = NULL;
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        char probe[96];
        snprintf(probe, sizeof(probe), "%s/EBOOT.PBP", dirs[i]);
        if (file_exists(probe)) {
            current = dirs[i];
            break;
        }
    }

    /* Fallback: create the standard deployment folder. */
    if (!current) {
        sceIoMkdir(SAVE_DIR, 0777);
        current = SAVE_DIR;
    }

    snprintf(save_path, sizeof(save_path), "%s/SAVE.BIN", current);

    /* One-time migration: if the new folder has no save yet, reuse an old one. */
    if (!file_exists(save_path)) {
        for (unsigned i = 1; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
            char old_save[96];
            snprintf(old_save, sizeof(old_save), "%s/SAVE.BIN", dirs[i]);
            if (file_exists(old_save)) {
                if (copy_file(old_save, save_path)) break;
            }
        }
    }
}

int save_request_save(void) {
    resolve_save_path();

    SaveHeader h;
    h.magic = SAVE_MAGIC;
    h.version = SAVE_VERSION;
    h.size = (uint32_t)sizeof(Game);
    h.checksum = hash_bytes(&g, sizeof(Game));

    SceUID fd = sceIoOpen(save_path,
                          PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        toast("SAVE FAILED");
        return 0;
    }
    int wrote_h = sceIoWrite(fd, &h, sizeof(h));
    int wrote_g = sceIoWrite(fd, &g, sizeof(g));
    sceIoClose(fd);
    if (wrote_h != (int)sizeof(h) || wrote_g != (int)sizeof(g)) {
        toast("SAVE FAILED");
        return 0;
    }
    toast("GAME SAVED");
    return 1;
}

int save_request_load(void) {
    resolve_save_path();

    SceUID fd = sceIoOpen(save_path, PSP_O_RDONLY, 0);
    if (fd < 0) {
        toast("NO SAVE FILE");
        return 0;
    }

    SaveHeader h;
    memset(&h, 0, sizeof(h));
    int got_h = sceIoRead(fd, &h, sizeof(h));
    int ok = 0;
    if (got_h == (int)sizeof(h) &&
        h.magic == SAVE_MAGIC &&
        h.version >= SAVE_MIN_VERSION &&
        h.version <= SAVE_VERSION &&
        h.size >= 1024u &&
        h.size <= (uint32_t)sizeof(Game)) {
        memset(&load_game, 0, sizeof(load_game));
        int got_g = sceIoRead(fd, &load_game, h.size);
        if (got_g == (int)h.size &&
            h.checksum == hash_bytes(&load_game, h.size)) {
            /* Version 2 had no cursor_speed_pct: old files end before it. */
            if (h.version < 3u) load_game.cursor_speed_pct = 100;
            ok = 1;
        }
    }
    sceIoClose(fd);

    if (!ok) {
        toast("LOAD FAILED");
        return 0;
    }

    g = load_game;
    game_after_load();
    return 1;
}

void save_init(void) {
    save_path[0] = 0;
}
