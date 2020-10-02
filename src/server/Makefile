CFLAGS += -I../include -pthread
LDFLAGS += -pthread
CC = gcc
TARGET = server
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
RM = rm -rf

all: clean build

build:
	$(CC) $(SRCS) -c $(CFLAGS) $(LDFLAGS)
	$(CC) $(OBJS) $(CFLAGS) $(LDFLAGS) -o $(TARGET)
	$(RM) $(OBJS)

clean:
	$(RM) $(OBJS) $(TARGET)
