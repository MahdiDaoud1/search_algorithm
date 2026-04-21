CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS = -lraylib -lm

# Platform detection
UNAME := $(shell uname -s)
ifeq ($(UNAME), Linux)
    LDFLAGS += -lGL -lpthread -ldl -lrt -lX11
endif
ifeq ($(UNAME), Darwin)
    LDFLAGS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif

SRC = src/main.c src/grid.c src/generators.c src/search.c src/render.c src/ui.c
OBJ = $(SRC:.c=.o)
TARGET = maze_search

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
