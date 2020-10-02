CFLAGS += -I../include -pthread
LDFLAGS += -pthread
CC = gcc
TARGET = client
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
RM = rm -rf

all: clean build

build: $(OBJS)
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $(TARGET)
	$(RM) $^

%.o: %.c
	$(CC) $^ -c $(CFLAGS) $(LDFLAGS)

clean:
	$(RM) $(OBJS) $(TARGET)
