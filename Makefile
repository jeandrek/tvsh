CFLAGS=-Wall -Wextra -std=c99 -O3

tvsh: tvsh.c
	$(CC) $(CFLAGS) -o $@ $<

install: tvsh
	install tvsh	/usr/local/bin
	install tvsh.1	/usr/share/man/man1

uninstall:
	rm /usr/local/bin/tvsh
	rm /usr/share/man/man1/tvsh.1

clean:
	rm -f tvsh
