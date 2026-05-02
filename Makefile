CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Iinclude -I$(LIBIN103)/include
LDFLAGS = -lraylib -lopengl32 -lgdi32 -lwinmm -lm

# Path to libin103
LIBIN103 = $(HOME)/Library/libin103-1.4.2

SRC = src/main.c src/grid.c src/generators.c src/traps.c \
      src/search.c src/render.c src/ui.c
OBJ = $(SRC:.c=.o)
TARGET = maze_search.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBIN103)/source/libin103.a $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
