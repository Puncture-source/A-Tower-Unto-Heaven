CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -O2
LDFLAGS = -lncurses -lm

SRC = src/main.c src/map.c src/entity.c src/items.c src/render.c
OBJ = $(SRC:.c=.o)
TARGET = tower

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

src/%.o: src/%.c src/game.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: all
	./$(TARGET)

.PHONY: all clean run
