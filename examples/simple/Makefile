CC = clang
CFLAGS = -std=gnu99 -DHS_IMPLEMENTATION
CFLAGS += -O0 -g

SRC = main.c
OUT = out

.PHONY: all compile debug clean valgrind

all: compile

compile:
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

debug: compile
	gdb -tui ./$(OUT)

valgrind: compile
	valgrind --leak-check=full ./$(OUT)

clean:
	rm -f $(OUT)

