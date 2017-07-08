CFLAGS=-Wall -Wextra -std=c99 -O3

tvsh: tvsh.c
	$(CC) $(CFLAGS) -o $@ $<

install: tvsh
	install tvsh	/usr/local/bin
	install tvsh.1	/usr/share/man/man1

uninstall:
	rm -f		/usr/local/bin/tvsh
	rm -f		/usr/share/man/man1/tvsh.1

clean:
	rm -f tvsh
