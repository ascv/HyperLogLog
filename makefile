CFLAGS = -Wall -g -lm -o
SRCS = loglog.c lib.c murmur3.c

all:
	gcc $(SRCS) $(CFLAGS) loglog

clean:
	rm loglog *o -f