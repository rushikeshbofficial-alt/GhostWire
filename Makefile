CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

# Detect operating system
ifeq ($(OS),Windows_NT)
    TARGET = p2p.exe
    TEST_TARGET = test_catalog.exe
    LIBS = -lws2_32
    CLEAN_CMD = if exist $(TARGET) del /Q /F $(TARGET) & if exist $(TEST_TARGET) del /Q /F $(TEST_TARGET) & if exist src\*.o del /Q /F src\*.o & if exist tests\*.o del /Q /F tests\*.o & if exist p2p del /Q /F p2p
else
    TARGET = p2p
    TEST_TARGET = test_catalog
    LIBS =
    CLEAN_CMD = rm -f $(TARGET) $(TEST_TARGET) src/*.o tests/*.o
endif

SRCS = src/main.c src/peer.c src/catalog.c src/protocol.c src/network.c src/transfer.c src/common.c
OBJS = $(SRCS:.c=.o)

TEST_SRCS = tests/test_catalog.c src/catalog.c src/common.c
TEST_OBJS = tests/test_catalog.o src/catalog.o src/common.o

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
ifeq ($(OS),Windows_NT)
	@copy /Y $(TARGET) p2p >nul 2>&1 || ver >nul
endif

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
ifeq ($(OS),Windows_NT)
	.\$(TEST_TARGET)
else
	./$(TEST_TARGET)
endif

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	@$(CLEAN_CMD)
