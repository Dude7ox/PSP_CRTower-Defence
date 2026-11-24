#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <math.h>
#include <string.h>

#include "game.h"
#include "render.h"
#include "audio.h"
#include "save.h"

PSP_MODULE_INFO("CRTOWERDEF", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(2048);
PSP_HEAP_THRESHOLD_SIZE_KB(512);

static volatile int running = 1;

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    running = 0;
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("crtd_exit", exit_callback, NULL);
    if (cbid >= 0) sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    int thid = sceKernelCreateThread("crtd_exit_thread", callback_thread,
                                     0x11, 0x1000, 0, NULL);
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

int main(void) {
    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    render_init();
    audio_init();
    game_init();
    save_init();

    SceCtrlData pad, prev;
    memset(&pad, 0, sizeof(pad));
    memset(&prev, 0, sizeof(prev));
    pad.Lx = pad.Ly = 128;

    const float dt = 1.0f / 60.0f;

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);

        uint32_t buttons = pad.Buttons;
        uint32_t pressed = buttons & ~prev.Buttons;

        float mx = ((float)pad.Lx - 128.0f) / 110.0f;
        float my = ((float)pad.Ly - 128.0f) / 110.0f;
        if (fabsf(mx) < 0.16f) mx = 0.0f;
        if (fabsf(my) < 0.16f) my = 0.0f;
        if (mx < -1.0f) mx = -1.0f;
        if (mx > 1.0f) mx = 1.0f;
        if (my < -1.0f) my = -1.0f;
        if (my > 1.0f) my = 1.0f;

        if (buttons & PSP_CTRL_LEFT) mx = -1.0f;
        if (buttons & PSP_CTRL_RIGHT) mx = 1.0f;
        if (buttons & PSP_CTRL_UP) my = -1.0f;
        if (buttons & PSP_CTRL_DOWN) my = 1.0f;

        game_update(dt, buttons, pressed, mx, my);

        render_frame_begin();
        game_draw();
        render_frame_end();

        prev = pad;
    }

    audio_shutdown();
    render_shutdown();
    sceKernelExitGame();
    return 0;
}
