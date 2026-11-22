TARGET = ArenaClash
OBJS = main.o
CFLAGS = -O2 -G0 -Wall
LIBS = -lm
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Arena Clash
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
