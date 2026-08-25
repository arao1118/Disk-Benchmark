# Get source file name without extension
SRC := $(wildcard *.cpp)
TARGET := $(basename $(SRC))

CC := g++
CFLAGS := -g -Wall -Wextra

all:
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
