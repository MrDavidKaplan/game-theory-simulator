CC = cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS ?= -Iinclude

.PHONY: all test clean

all: build/game-theory

build:
	mkdir -p build

build/game-theory: src/main.c src/game.c include/game.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/game.c -o $@

build/test_game: tests/test_game.c src/game.c include/game.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_game.c src/game.c -o $@

test: build/test_game build/game-theory
	./build/test_game
	./build/game-theory play --p1 copycat --p2 grudger --rounds 20 --seed 7 >/dev/null
	./build/game-theory tournament --rounds 10 --matches 3 --seed 7 >/dev/null
	printf "1\n0\n" | ./build/game-theory human --rounds 1 >/dev/null
	printf "0\n" | ./build/game-theory vs-ai --rounds 1 --p2 copycat >/dev/null
	@echo "CLI smoke tests passed."

clean:
	rm -rf build
