SRC_DIR = http_server

OUTPUT = http_server.h
PUB = http_server.h
PRIVATE = http_server_internal.h
SRC = http_server.c buffer.c http_parser.c logger.c error.c http_response.c

MACRO = HTTP_SERVER

.PHONY: all build

all: build

build:
	python3 build.py \
		--macro $(MACRO) \
		--output $(OUTPUT) \
		--pub $(addprefix $(SRC_DIR)/, $(PUB)) \
		--src $(addprefix $(SRC_DIR)/, $(SRC)) \
		--private $(addprefix $(SRC_DIR)/, $(PRIVATE))


