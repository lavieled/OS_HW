CC = gcc
CFLAGS = -std=c99 -Wall -Werror -pedantic-errors -DNDEBUG -pthread

# target exe
TARGET = smash

# source files
SRCS = smash.c commands.c signals.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

# link
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

# compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
