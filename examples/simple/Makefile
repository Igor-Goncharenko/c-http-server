CC = clang
CFLAGS = -std=gnu99 -DHTTP_SERVER_IMPLEMENTATION
CFLAGS += -O0 -g

SRC = main.c
OUT = out

.PHONY: all compile debug clean

all: compile debug clean

compile:
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

debug:
	gdb -tui ./$(OUT)

clean:
	rm -f $(OUT)

