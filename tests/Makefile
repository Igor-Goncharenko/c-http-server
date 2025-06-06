OUT = out

CC = clang
CFLAGS = -std=gnu99  -O3 
CFLAGS += -I../http_server -Itests_util

SRC = test_http_parser.c test_http_server.c test_buffer.c
PROGS = $(patsubst %.c,%,$(SRC))

all: clean $(PROGS) $(addprefix run_,$(PROGS)) clean

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

run_%: %
	./$<

clean:
	rm -f $(PROGS)

