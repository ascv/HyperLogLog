CFLAGS = -Wall -g -lm
OBJS = loglog.o
SRCS = loglog.c hash.c
HDRS = loglog.h

all:
	gcc $(SRCS) $(CFLAGS) -o loglog

clean:
	rm loglog *o -f