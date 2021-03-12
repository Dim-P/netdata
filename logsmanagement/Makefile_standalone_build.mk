SRC_DIR := .
OBJ_DIR := obj

EXE := netdata-logs
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CFLAGS   := -Wall 
LDFLAGS  := -Llib
LDLIBS   := -luv -lsqlite3 -llz4

STRESS_TEST ?= 0
CFLAGS += -DSTRESS_TEST=$(STRESS_TEST)

DEBUG_LEV ?= 0
CFLAGS += -DDEBUG_LEV=$(DEBUG_LEV)
ifeq ($(DEBUG_LEV), 0)
    CFLAGS += -O3
else
    CFLAGS += -g -O0
endif

.PHONY: all clean

all: $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	@$(RM) -rv $(OBJ_DIR) $(EXE)
