# ----------------------------------------
# group nb 138
# 28871600 : Heuschling Thomas
# 28751600 : d'Herbais de Thun Sebastien
# ----------------------------------------

# Tools
GCC = gcc
RM = rm
VERSION = gnu89

# Binary names
OUT = trtp_receiver
TEST = trtp_test

SRC_DIR = ./src
TEST_DIR = ./tests
BIN_DIR = ./bin

# Source file
SRC := $(basename $(shell find $(SRC_DIR) -name *.c))
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
LDFLAGS = -lz -lpthread
FLAGS = -Werror -std=$(VERSION)
RELEASE_FLAGS = -O3
DEBUG_FLAGS = -O0 -ggdb

# does not need verification
.PHONY: clean report stat install_tectonic

# main
all: clean build run

# build
build: $(OBJECTS)
	$(GCC) $(FLAGS) $(OBJECTS) -o $(BIN_DIR)/$(OUT) $(LDFLAGS)

test_build: FLAGS += $(DEBUG_FLAGS)
test_build: build

# release build
release: FLAGS += $(RELEASE_FLAGS)
release: build

# run
run:
	$(BIN_DIR)/$(OUT) -o $(BIN_DIR)/%d -n 3 -w 31 :: 5555

# Build and run tests
test: FLAGS += $(DEBUG_FLAGS)
test: build
test: LDFLAGS += -lcunit 
test: $(TEST_OBJECTS)
	$(GCC) $(FLAGS) $(ALL) $(TEST_MAIN) -o $(BIN_DIR)/$(TEST) $(LDFLAGS)
	$(BIN_DIR)/$(TEST)

# build individual files
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(GCC) $(FLAGS) -c $< -o $@ $(LDFLAGS)

$(BIN_DIR)/%.o: $(TEST_DIR)/%.c
	$(GCC) $(FLAGS) -c $< -o $@ $(LDFLAGS)

# Cleaning stuff
clean: 
	$(RM) -f $(BIN)

# Generated gitlog.stat
stat:
	git log --stat > gitlog.stat

# Read tectonic github repo for dependencies
install_tectonic:
	cargo install tectonic

# Build the report using tectonic
report:
	cd report && tectonic main.tex
	cp ./report/main.pdf ./report.pdf

valgrind: FLAGS += $(DEBUG_FLAGS)
valgrind: build
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose \
		$(BIN_DIR)/$(OUT) -n 4 -o $(BIN_DIR)/%d :: 5555 2> $(BIN_DIR)/valgrind.txt

helgrind: FLAGS += $(DEBUG_FLAGS)
helgrind: build
	valgrind --tool=helgrind \
		$(BIN_DIR)/$(OUT) -n 4 -o $(BIN_DIR)/%d :: 5555 2> $(BIN_DIR)/helgrind.txt

memcheck: FLAGS += $(DEBUG_FLAGS)
memcheck: build
	valgrind --tool=memcheck --track-origins=yes \
		$(BIN_DIR)/$(OUT) -n 4 -o $(BIN_DIR)/%d :: 5555 2> $(BIN_DIR)/memcheck.txt

callgrind: FLAGS += $(DEBUG_FLAGS)
callgrind: build
	@echo '----------------------------------------------------------'
	@echo 'This function runs valgrind followed by gprof2dot.'
	@echo 'It is used to generate the callgraph of the application'
	@echo 'profile CPU time. In order to fully test, you also need'
	@echo 'to start a sender on the side after valgrind has started!'
	@echo '----------------------------------------------------------'

	valgrind --tool=callgrind --callgrind-out-file=$(BIN_DIR)/callgrind.txt \
		$(BIN_DIR)/$(OUT) -n 4 -w 1 -o $(BIN_DIR)/%d :: 5555
		
	@echo '----------------------------------------------------------'

plot:
	./tools/gprof2dot.py -f callgrind $(BIN_DIR)/callgrind.txt | dot -Tpng -o callgraph.png

debug: FLAGS += $(DEBUG_FLAGS)
debug: build
	gdb -ex run --args $(BIN_DIR)/$(OUT) -o $(BIN_DIR)/%d :: 5555

tcpdump:
	sudo tcpdump -s 0 -i lo udp port 5555 -w ./bin/udpdump.pcap 