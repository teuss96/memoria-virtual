CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g -pthread

vm: vm.c
	$(CC) $(CFLAGS) -o $@ vm.c

# gabarito do professor, modelo de referencia e casos de erro
test: vm
	sh testes/run.sh ./vm

# mesma suite com AddressSanitizer + UBSan e depois com ThreadSanitizer
sanitize: vm.c
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=all -o vm_asan vm.c
	sh testes/run.sh ./vm_asan
	$(CC) $(CFLAGS) -fsanitize=thread -o vm_tsan vm.c
	sh testes/run.sh ./vm_tsan

clean:
	rm -f vm vm_asan vm_tsan correct.txt

.PHONY: test sanitize clean
