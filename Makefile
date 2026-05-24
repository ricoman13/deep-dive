TARGET		:= DeepDiveVita
SOURCES		:= src
CFILES		:= $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OBJS		:= $(CFILES:.c=.o)

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc

CFLAGS  = -Wl,-q -Wall -O2 -std=c99 \
          -I$(VITASDK)/arm-vita-eabi/include/SDL2

LIBS    = -lSDL2main -lSDL2 \
          -lSceDisplay_stub \
          -lSceGxm_stub \
          -lSceSysmodule_stub \
          -lSceCtrl_stub \
          -lSceAudio_stub \
          -lSceAudioIn_stub \
          -lSceKernelDmacMgr_stub \
          -lSceNet_stub \
          -lSceNetCtl_stub \
          -lSceHid_stub \
          -lSceTouch_stub \
          -lScePvf_stub \
          -lm -lpthread -lc

all: $(TARGET).vpk

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

eboot.elf: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TARGET).velf: eboot.elf
	vita-elf-create $< $@

eboot.bin: $(TARGET).velf
	vita-make-fself $< $@

param.sfo:
	vita-mksfoex -s TITLE_ID="DPDV00001" "DeepDive Vita" $@

$(TARGET).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add sce_sys/icon0.png=sce_sys/icon0.png \
		$(TARGET).vpk

clean:
	rm -f $(OBJS) eboot.elf $(TARGET).velf eboot.bin param.sfo $(TARGET).vpk

.PHONY: all clean
