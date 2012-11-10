CFLAGS = -Wall -g -lm
OBJS = loglog.o
SRCS = loglog.c hash.c

all:
	gcc $(SRCS) $(CFLAGS) -o loglog

clean:
	rm loglog *o -f