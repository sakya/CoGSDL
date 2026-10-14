TARGET=CoGSDL
VERSION=1.0

# Miyoo Mini (Onion) build: make PLATFORM=miyoo
# Run it inside the union-miyoomini-toolchain container.
PLATFORM?=

ifeq ($(PLATFORM), miyoo)
	CROSS_COMPILE?=arm-linux-gnueabihf-
	CC=$(CROSS_COMPILE)gcc
	# SDL2 headers and libraries for Miyoo: miyoo/include and miyoo/lib
	MIYOO_DIR?=miyoo
	SDL2FLAGS=-L$(MIYOO_DIR)/lib -Wl,-rpath-link,$(MIYOO_DIR)/lib -Wl,--allow-shlib-undefined \
		-l:libSDL2-2.0.so.0 -l:libSDL2_image-2.0.so.0 -l:libSDL2_ttf-2.0.so.0 -l:libSDL2_mixer-2.0.so.0
	CFLAGS+=-I$(INC_DIR) -I$(MIYOO_DIR)/include -I$(MIYOO_DIR)/include/SDL2 -D_REENTRANT -DMIYOO -Wall -O2 -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
	LDFLAGS=-lm -lpthread
else
	CC=gcc
	SDL2FLAGS=$(shell pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf SDL2_mixer)
	ifeq ($(OS), Windows_NT)
		CFLAGS+=-I$(INC_DIR) -Wall -g
		LDFLAGS=-lm
	else
		CFLAGS+=-I$(INC_DIR) -Wall -g -fsanitize=address
		LDFLAGS=-lm -fsanitize=address
	endif
endif

RM=rm
RMFLAGS=-rf

MKDIR=mkdir
MKDIRFLAGS=-p

OUTPUT_DIR=build
INC_DIR=include
SRC_DIR=src
OBJ_DIR=objs

SRCS=$(wildcard $(SRC_DIR)/*.c)
OBJS=$(addprefix $(OBJ_DIR)/, $(notdir $(SRCS:.c=.o)))

all: $(TARGET)

re: clean all

clean:
	$(RM) $(RMFLAGS) $(OBJS) $(TARGET) CoGSDL-v$(VERSION).zip

$(TARGET): ${OBJS}
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) $(SDL2FLAGS) -o $(TARGET)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	@$(MKDIR) $(MKDIRFLAGS) $(OBJ_DIR)

pack: CoGSDL-v$(VERSION).zip

CoGSDL-v$(VERSION).zip: $(TARGET)
	zip -r CoGSDL-v$(VERSION).zip fonts images miyoo sounds CoGSDL icon config.json launch.sh