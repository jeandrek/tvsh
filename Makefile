CFLAGS=-Wall -Wextra -pedantic -std=c99 -O3

all: tvsh

.PHONY: clean

clean:
	rm -f tvsh
