CFLAGS = -Wall -g -lm -o
SRCS = hll.c loglog.c lib.c murmur3.c

all:
	gcc $(SRCS) $(CFLAGS) hll

clean:
	rm loglog *o -f

