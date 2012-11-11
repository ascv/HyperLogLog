CFLAGS = -Wall -g -lm
OBJS = loglog.o
SRCS = loglog.c hash.c murmur3.c

all:
	gcc $(SRCS) $(CFLAGS) -o loglog

clean:
	rm loglog *o -f