CC := cc
CFLAGS := -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -std=c11 -O2 -Wall -Wextra -Iinclude
LDLIBS := -lpthread -lcrypto -libverbs -lrdmacm
BIN_DIR := bin
SRC_DIR := src

COMMON_SRCS := \
	$(SRC_DIR)/common.c \
	$(SRC_DIR)/config.c \
	$(SRC_DIR)/layout.c \
	$(SRC_DIR)/crypto.c \
	$(SRC_DIR)/cluster.c \
	$(SRC_DIR)/transport_tcp.c \
	$(SRC_DIR)/transport_rdma.c

all: $(BIN_DIR)/cn $(BIN_DIR)/mn

$(BIN_DIR)/cn: $(COMMON_SRCS) $(SRC_DIR)/cn_main.c
	$(CC) $(CFLAGS) -o $@ $(COMMON_SRCS) $(SRC_DIR)/cn_main.c $(LDLIBS)

$(BIN_DIR)/mn: $(COMMON_SRCS) $(SRC_DIR)/mn_main.c
	$(CC) $(CFLAGS) -o $@ $(COMMON_SRCS) $(SRC_DIR)/mn_main.c $(LDLIBS)

clean:
	rm -f $(BIN_DIR)/cn $(BIN_DIR)/mn

.PHONY: all clean
