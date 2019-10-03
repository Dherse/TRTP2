# ----------------------------
# group nb 138
# 28871600 : Heuschling Thomas
# 28751600 : d'Herbais de Thun Sebastien
# ----------------------------

GCC = gcc
RM = rm
DEL = del

OUT = trtp_receiver
TEST = trtp_test

SRC_DIR = ./src
TEST_DIR = ./tests
HEADER_DIR = ./headers
BIN_DIR = ./bin

SRC = $(wildcard $(SRC_DIR)/*.c)
HEADERS = $(wildcard $(HEADER_DIR)/*.h)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRC))

TESTS = $(TEST_DIR)/tests.c
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c, $(BIN_DIR)/%.o, $(TESTS))

BIN = $(wildcard $(BIN_DIR)/*.o)
BIN_TEST = $(filter-out $(BIN_DIR)/main.o,$(wildcard $(BIN_DIR)/*.o))
BIN_DEL = $(filter-out $(BIN_DIR)/.gitkeep, $(wildcard $(BIN_DIR)/*))

LDFLAGS = -lz -lpthread
FLAGS = -Werror
RELEASE_FLAGS = -O3
DEBUG_FLAGS = -O0 -ggdb

.PHONY: clean report

all: clean
all: 
	@echo 'sources: $(SRC)'
	@echo 'headers: $(HEADERS)'
	@echo 'objects: $(OBJECTS)'
	@echo 'tests  : $(TEST_OBJECTS)'
	make run

# Building stuff

release: clean
release: build

run: FLAGS += $(RELEASE_FLAGS)
run: build
	$(GCC) $(FLAGS) $(BIN) -o $(BIN_DIR)/$(OUT) $(LDFLAGS)
	$(BIN_DIR)/$(OUT)

build: $(OBJECTS)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(GCC) $(FLAGS) -c $< -o $@ $(LDFLAGS)

# Testing stuff

debug: FLAGS += $(DEBUG_FLAGS)
debug: build

test: debug
test: FLAGS += $(DEBUG_FLAGS)
test: LDFLAGS += -lcunit
test: $(TEST_OBJECTS)
	$(GCC) $(FLAGS) $(BIN_TEST) -o $(BIN_DIR)/$(TEST) $(LDFLAGS)
	$(BIN_DIR)/$(TEST)

$(BIN_DIR)/%.o: $(TEST_DIR)/%.c
	$(GCC) $(FLAGS) -c $< -o $@ $(LDFLAGS)

# Cleaning stuff

clean: 
	$(RM) -f $(BIN_DEL)

stat:
	git log --stat > gitlog.stat

# Read mdbook & mdbook-latex github repos for dependencies
install_mdbook:
	cargo install mdbook
	cargo install mdbook-latex

report:
	cd report && mdbook build
	cp report/book/LINGI1341\ -\ Rapport\ de\ projet.pdf ./report.pdf