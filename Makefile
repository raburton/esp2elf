CC=gcc
CFLAGS=-I. -g
DEPS = esp2elf.h bootrom_symbols.h bootrom.h
OBJ = esp2elf.o
LIBS = -lelf

esp2elf: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

bootrom.h: bootrom.bin
	xxd -i $< > $@

.PHONY: clean

clean:
	rm -f esp2elf *.o bootrom.h

