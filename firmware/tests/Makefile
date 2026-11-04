# Testes do núcleo no PC (gcc nativo).
CFLAGS = -std=c99 -O2 -Wall -Wextra -Werror -I../core
SRC = ../core/decoder.c ../core/ignition.c ../core/table.c ../core/config.c ../core/sensors.c test_core.c

test_core: $(SRC) $(wildcard ../core/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm

test: test_core
	./test_core

clean:
	rm -f test_core

.PHONY: test clean
