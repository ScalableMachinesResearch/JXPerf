
SRCS=$(wildcard *.c)


OBJS=$(SRCS:.c=.o)


libwatchpoint.a: $(OBJS)
	ar rcs $@ $^

%.o: %.c
	gcc -std=gnu99 -fPIC -c -O3 -o $@ $< -I../include -I.

clean:
	rm -r libwatchpoint.a $(OBJS)
