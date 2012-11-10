CFLAGS = -Wall -g -lm
OBJS = loglog.o
SRCS = loglog.c hash.c
HDRS = loglog.h

all:
	gcc loglog.c hash.c loglog.h $(CFLAGS) -o loglog

clean:
	rm loglog *o -f