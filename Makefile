# ----------------------------
# group nb 138
# 28871600 : Heuschling Thomas
# 28751600 : d'Herbais de Thun Sebastien
# ----------------------------

GCC = gcc
RM = rm
DEL = del

OUT = trtp_receiver

SRC_DIR = ./src
HEADER_DIR = ./headers
BIN_DIR = ./bin

SRC = $(wildcard $(SRC_DIR)/*.c)
HEADERS = $(wildcard $(HEADER_DIR)/*.h)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRC))

BIN = $(wildcard $(BIN_DIR)/*.o)

.PHONY: clean

all: 
	@echo 'sources: "$(SRC)"'
	@echo 'headers: "$(HEADERS)"'
	@echo 'objects: "$(OBJECTS)"'
	make run

run: build
	$(GCC) $(BIN) -o $(BIN_DIR)/$(OUT)
	$(BIN_DIR)/$(OUT)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(GCC) -c $< -o $@

build: $(OBJECTS)

clean: 
	$(RM) $(BIN)
	$(RM) $(BIN_DIR)/$(OUT)

clean_win:
	$(DEL) $(BIN)