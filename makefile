CFLAGS = -Wall -g -lm
OBJS = loglog.o
SRCS = loglog.c lib.c murmur3.c

all:
	gcc $(SRCS) $(CFLAGS) -o loglog

clean:
	rm loglog *o -f