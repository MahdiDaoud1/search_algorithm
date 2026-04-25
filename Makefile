CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Iinclude -D_POSIX_C_SOURCE=199309L
LDFLAGS = -lraylib -lopengl32 -lgdi32 -lwinmm -lm

SRC = src/main.c src/grid.c src/generators.c src/traps.c src/search.c src/render.c src/ui.c
OBJ = $(SRC:.c=.o)
TARGET = maze_search.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
