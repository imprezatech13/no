# Build with devkitARM + libctru (use the devkitpro/devkitarm docker image or a local devkitPro install)
#   make          -> roblox2013.3dsx  (Homebrew Launcher; most forgiving)
#   make cia      -> roblox2013.cia   (needs makerom + bannertool on PATH)

ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO is not set (use the devkitpro/devkitarm docker image or install devkitPro)")
endif
export PATH := $(DEVKITPRO)/devkitARM/bin:$(DEVKITPRO)/tools/bin:$(PATH)

TARGET  := roblox2013
CC      := arm-none-eabi-gcc
ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS  := -g -Wall -O2 -mword-relocations -fomit-frame-pointer -ffunction-sections \
           $(ARCH) -D__3DS__ -std=gnu11 -I$(DEVKITPRO)/libctru/include
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(TARGET).map -L$(DEVKITPRO)/libctru/lib
LIBS    := -lctru -lm

NAME  := ROBLOX 2013
DESC  := 2013-style ROBLOX for New 3DS
AUTH  := Fan project

all: $(TARGET).3dsx

$(TARGET).elf: source/main.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LIBS)

$(TARGET).smdh: gfx/icon.png
	smdhtool --create "$(NAME)" "$(DESC)" "$(AUTH)" gfx/icon.png $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	3dsxtool $< $@ --smdh=$(TARGET).smdh

banner.bnr: gfx/banner.png gfx/audio.wav
	bannertool makebanner -i gfx/banner.png -a gfx/audio.wav -o $@

icon.icn: gfx/icon.png
	bannertool makesmdh -s "$(NAME)" -l "$(DESC)" -p "$(AUTH)" -i gfx/icon.png -o $@

cia: $(TARGET).cia
$(TARGET).cia: $(TARGET).elf banner.bnr icon.icn cia.rsf
	makerom -f cia -o $@ -elf $(TARGET).elf -rsf cia.rsf -icon icon.icn -banner banner.bnr \
	        -exefslogo -target t

clean:
	rm -f $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh $(TARGET).cia $(TARGET).map banner.bnr icon.icn

.PHONY: all cia clean
