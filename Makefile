TARGET = crtower
OBJS = src/main.o src/render.o src/game.o src/audio.o src/save.o

CFLAGS = -O2 -G0 -Wall -Wextra -std=c99
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgum_vfpu -lpspvfpu -lpspgu -lpspaudio -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_LARGE_MEMORY = 0
PSP_EBOOT_TITLE = CRTower Defence
PSP_EBOOT_ICON = ICON0.PNG
SFOFLAGS += -s DISC_ID=CRTD00001

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
