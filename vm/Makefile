CFILES += ../util/string.c

CFILES += ../elf/elf.c
CFILES += ../elf/elf_link.c
CFILES += ../elf/elf_native.c
CFILES += ../elf/elf_native32.c
CFILES += ../elf/elf_x64.c
CFILES += ../elf/elf_x64_so.c
CFILES += ../elf/elf_arm64.c
CFILES += ../elf/elf_arm64_so.c
CFILES += ../elf/elf_arm32.c
CFILES += ../elf/elf_arm32_so.c
CFILES += ../elf/elf_naja.c
CFILES += ../elf/elf_naja_so.c
CFILES += ../elf/dwarf.c
CFILES += ../elf/dwarf_abbrev.c
CFILES += ../elf/dwarf_info.c
CFILES += ../elf/dwarf_line.c

CFILES += ../vm/vm.c
CFILES += ../vm/vm_naja.c
CFILES += ../vm/vm_naja_asm.c
CFILES += ../vm/main.c

CFLAGS += -g
#CFLAGS += -Wall 
CFLAGS += -I../util
CFLAGS += -I../core
CFLAGS += -I../lex
CFLAGS += -I../parse
CFLAGS += -I../elf
CFLAGS += -I../vm
CFLAGS += -I../native
CFLAGS += -I../native/x64
CFLAGS += -I../native/risc

LDFLAGS += -ldl

all:
	gcc $(CFLAGS) $(CFILES) $(LDFLAGS) -o nvm

clean:
	rm *.o
