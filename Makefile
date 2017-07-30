CFLAGS=-Wall -Wextra -std=c99 -pedantic -O3

all: tvsh

.PHONY: clean

clean:
	rm -f tvsh
