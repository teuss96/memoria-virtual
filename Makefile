CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g -pthread

vm: vm.c
	$(CC) $(CFLAGS) -o $@ vm.c

clean:
	rm -f vm correct.txt

.PHONY: clean
