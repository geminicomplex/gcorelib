#
# libgcore build script
#
# Do not build for linux or arm on a mac system
# and mac on a linux system.
#

BUILD_PATH :=
INCLUDES :=-I. -I./board -I../driver -I./lib/jsmn -I./lib/avl -I./lib/progress -I./lib/lz4 -I./lib/capnp 
CFLAGS :=-O2 -c -fPIC -Wall -funwind-tables -g -ggdb
CC := gcc
LDFLAGS := -fPIC
EXEC :=
OS := $(shell uname)
PLAT :=

ifeq ($(OS),Darwin)
	PLAT := mac
	BUILD_PATH := build/macosx
	LDFLAGS += -shared -dynamiclib -Wl,-install_name,libgcore.dylib -lpthread
	EXEC += $(BUILD_PATH)/libgcore.dylib
else
ifeq ($(MAKECMDGOALS),arm)
	PLAT := arm
	BUILD_PATH := build/arm
	ARM_CFLAGS := -march=armv7-a -mcpu=cortex-a9 -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard
	CFLAGS := ${ARM_CFLAGS} $(CFLAGS) -D_FILE_OFFSET_BITS=64
	CC := arm-linux-gnueabihf-gcc
	LDFLAGS += -shared -Wl,-soname,libgcore.so -lpthread
	EXEC += $(BUILD_PATH)/libgcore.so
else
	PLAT := linux
	BUILD_PATH := build/linux
	LDFLAGS += -shared -Wl,-soname,libgcore.so -lpthread
	EXEC += $(BUILD_PATH)/libgcore.so
endif
endif

SRCS := common.c dots.c util.c board/gpio.c board/artix.c board/i2c.c \
	   board/helper.c board/dma.c board/subcore.c board/dev.c subvec.c \
	   serialize/stim_serdes.capnp.c config.c lib/capnp/capn.c lib/capnp/capn-malloc.c \
	   lib/capnp/capn-stream.c lib/lz4/lz4hc.c lib/lz4/lz4frame.c lib/lz4/xxhash.c \
	   lib/lz4/lz4.c lib/jsmn/jsmn.c lib/avl/avl.c lib/slog/slog.c profile.c stim.c \
	   prgm.c

HEADERS := profile.h stim.h config.h board/dma.h board/helper.h board/subcore.h board/dev.h \
		board/gpio.h board/artix.h board/i2c.h serialize/stim_serdes.capnp.h dots.h common.h \
		subvec.h util.h lib/capnp/capnp_priv.h lib/capnp/capnp_c.h lib/lz4/xxhash.h lib/lz4/lz4.h \
		lib/lz4/lz4frame_static.h lib/lz4/lz4hc.h lib/lz4/lz4frame.h lib/jsmn/jsmn.h \
		lib/avl/avl.h lib/slog/slog.h prgm.h libgcore.h

#
# Don't run anything if all is given
#
all:
	@echo "error: must give a platform: mac, linux, arm"

#
# Must give target to run
#
mac: build/macosx/libgcore.dylib 
linux: build/linux/libgcore.so 
arm: build/arm/libgcore.so 

#
# Build mac, linux and arm. Must run clean before.
#
build/macosx/libgcore.dylib: $(SRCS) $(HEADERS)
	$(CC) $(INCLUDES) $(LDFLAGS) $(SRCS) -o build/macosx/libgcore.dylib

build/linux/libgcore.so: $(SRCS) $(HEADERS)
	$(CC) $(INCLUDES) $(LDFLAGS) $(SRCS) -o build/linux/libgcore.so

build/arm/libgcore.so: $(SRCS) $(HEADERS)
	$(CC) $(INCLUDES) $(LDFLAGS) $(SRCS) -o build/arm/libgcore.so

%.c: %.h
	touch $@

clean-mac:
	$(shell find . -name '*_mac.o' -delete)
	rm -f build/macosx/libgcore.dylib

clean-linux:
	$(shell find . -name '*_linux.o' -delete)
	rm -f build/linux/libgcore.so

clean-arm:
	$(shell find . -name '*_arm.o' -delete)
	rm -f build/arm/libgcore.so

clean: clean-mac clean-linux clean-arm

.PHONY : all mac linux arm clean clean-mac clean-linux clean-arm
