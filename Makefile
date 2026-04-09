CC := gcc
CFLAGS := -Wall -Wextra -Werror -g -Isrc/include
LDLIBS  := -lcurl
LDFLAGS := 
SRC_DIR := src
OBJ_DIR := obj
TARGET := tel-speed

SRCS := $(shell find $(SRC_DIR) -name "*.c")
OBJ := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)