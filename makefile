loglog:
	gcc -Wall -g loglog.h loglog.c murmur.c -lm -o loglog

all: 
	gcc -Wall -g loglog.h loglog.c hash.c -lm -o loglog

clean: 
	rm loglog *~ -f


