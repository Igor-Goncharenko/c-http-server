SRC_DIR = http_server

OUTPUT = http_server.h
PUB = http_server.h
PRIVATE = http_server_internal.h
SRC = $(wildcard $(SRC_DIR)/*.c)

MACRO = HS

.PHONY: all build

all: build

build:
	python3 build.py \
		--macro $(MACRO) \
		--output $(OUTPUT) \
		--pub $(addprefix $(SRC_DIR)/, $(PUB)) \
		--src $(SRC) \
		--private $(addprefix $(SRC_DIR)/, $(PRIVATE))


