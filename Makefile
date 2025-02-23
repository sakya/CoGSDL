TARGET=CoGSDL
VERSION=1.0

CC=gcc
SDL2FLAGS=$(shell pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf SDL2_mixer)
CFLAGS+=-I$(INC_DIR) -Wall -g -fsanitize=address
LDFLAGS=-lm -fsanitize=address

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
	zip -r CoGSDL-v$(VERSION).zip fonts images sounds CoGSDL icon config.json launch.sh