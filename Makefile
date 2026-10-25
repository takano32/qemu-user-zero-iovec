CC ?= cc
CFLAGS ?= -O2 -Wall
LDFLAGS ?= -static

zero-len-iovec: zero-len-iovec.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f zero-len-iovec

.PHONY: clean
