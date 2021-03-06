# ----------------------------------------
# group nb 138
# 28871600 : Heuschling Thomas
# 28751600 : d'Herbais de Thun Sebastien
# ----------------------------------------

# Tools
GCC = gcc
RM = rm
VERSION = gnu11

# Binary names
OUT = ./receiver
TEST = trtp_test

ARCHIVE = projet1_d-Herbais-de-Thun_Heuschling.zip

SRC_DIR = ./src
TEST_DIR = ./tests
BIN_DIR = ./bin

# Source file
SRC := $(basename $(shell find $(SRC_DIR) -name *.c))
SRCS := $(shell find $(SRC_DIR) -name *.c)
OBJECTS := $(SRC:$(SRC_DIR)/%=$(BIN_DIR)/%.o)

# Tests
TEST_SRC := $(basename $(shell find $(TEST_DIR) -name *.c))
TEST_OBJECTS := $(TEST_SRC:$(TEST_DIR)/%=$(BIN_DIR)/%.o)

# All
TEST_MAIN = $(BIN_DIR)/tests.o
MAIN = $(BIN_DIR)/main.o
MAINS = $(TEST_MAIN) $(MAIN)
ALL = $(filter-out $(MAINS), $(OBJECTS) $(TEST_OBJECTS))

# Binaries (to clean)
BIN := $(wildcard $(BIN_DIR)/*)

# Compiler flags
LDFLAGS = -lpthread -lrt
FLAGS = -Werror -std=$(VERSION)
RELEASE_FLAGS = -O3
DEBUG_FLAGS = -O0 -ggdb -DDEBUG

# does not need verification
.PHONY: clean report stat install_tectonic

# main
all: clean build

# build
build: $(OBJECTS)
	cd lib && make all
	$(GCC) $(FLAGS) ./lib/Crc32.o $(OBJECTS) -o $(OUT) $(LDFLAGS)

test_build: FLAGS += $(DEBUG_FLAGS)
test_build: LDFLAGS += -lcunit 
test_build: build
test_build: $(TEST_OBJECTS)
	$(GCC) $(FLAGS) ./lib/Crc32.o $(ALL) $(TEST_MAIN) -o $(BIN_DIR)/$(TEST) $(LDFLAGS)
	
# release build
release: FLAGS += $(RELEASE_FLAGS)
release: build

clang:
	cd lib && make all
	clang $(SRCS) ./lib/Crc32.o -Wall -Wpedantic -Wextra -Werror -std=$(VERSION) -Ofast -march=native -lpthread -o bin/receiver

# run
run:
	$(OUT) -o $(BIN_DIR)/%d -n 3 -N 1 -W 31 -m 100 :: 64536

# Build and run tests
test: FLAGS += $(DEBUG_FLAGS)
test: LDFLAGS += -lcunit 
test: test_build
	$(BIN_DIR)/$(TEST)

# build individual files
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(GCC) $(FLAGS) -c $< -o $@ $(LDFLAGS)

$(BIN_DIR)/%.o: $(TEST_DIR)/%.c
	$(GCC) $(FLAGS) -c $< -o $@ $(LDFLAGS)

# Cleaning stuff
clean: 
	cd lib && make clean
	$(RM) -f $(BIN)
	$(RM) -f $(OUT)

# Generated gitlog.stat
stat:
	git log --stat > gitlog.stat

# Read tectonic github repo for dependencies
install_tectonic:
	@echo 'Installing dependencies'
	sudo apt-get install libfontconfig1-dev libgraphite2-dev libharfbuzz-dev libicu-dev libssl-dev zlib1g-dev
	@echo 'Installing tectonic'
	cargo install tectonic

# Build the report using tectonic
report:
	cd report && tectonic main.tex
	cp ./report/main.pdf ./rapport.pdf

valgrind: FLAGS += $(DEBUG_FLAGS)
valgrind: build
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose \
		$(OUT) -o $(BIN_DIR)/%d -m 100 -n 3 -N 1 :: 64536 2> $(BIN_DIR)/valgrind.txt

helgrind: FLAGS += $(DEBUG_FLAGS)
helgrind: build
	valgrind --tool=helgrind \
		$(OUT) -o $(BIN_DIR)/%d -m 100 -n 3 -N 1 :: 64536 2> $(BIN_DIR)/helgrind.txt

memcheck: FLAGS += $(DEBUG_FLAGS)
memcheck: build
	valgrind --tool=memcheck --track-origins=yes \
		$(OUT) -o $(BIN_DIR)/%d -m 100 :: 64536 2> $(BIN_DIR)/memcheck.txt

callgrind: FLAGS += -O3 -ggdb
callgrind: build
	@echo '----------------------------------------------------------'
	@echo 'This function runs valgrind followed by gprof2dot.'
	@echo 'It is used to generate the callgraph of the application'
	@echo 'profile CPU time. In order to fully test, you also need'
	@echo 'to start a sender on the side after valgrind has started!'
	@echo '----------------------------------------------------------'

	valgrind --tool=callgrind --callgrind-out-file=$(BIN_DIR)/callgrind.txt \
		$(OUT) -s -w 31 -o $(BIN_DIR)/%d :: 64536
		
	@echo '----------------------------------------------------------'

plot:
	./tools/gprof2dot.py -f callgrind $(BIN_DIR)/callgrind.txt | dot -Tpng -o callgraph.png

debug: test_build
	gdb -ex run --args $(OUT) -n 3 -N 1 -m 100 -W 124 -o $(BIN_DIR)/%d :: 64536

tcpdump:
	sudo tcpdump -s 0 -i enp9s0 udp port 64536 -w ./bin/udpdump.pcap

archive: clean
archive: report
	$(RM) -f $(ARCHIVE)
	zip -r $(ARCHIVE) ./*