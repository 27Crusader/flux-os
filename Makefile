CC = g++
LD = ld
QEMU = qemu-system-i386

CFLAGS = -m32 -c -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
LDFLAGS = -m elf_i386 -T linker.ld

all: run

boot.o: boot.asm
	nasm -f elf32 boot.asm -o boot.o

kernel.o: kernal.cpp
	$(CC) $(CFLAGS) kernal.cpp -o kernel.o

flux_os.bin: boot.o kernel.o
	$(LD) $(LDFLAGS) -o flux_os.bin boot.o kernel.o

run: flux_os.bin
	$(QEMU) -kernel flux_os.bin -display curses

clean:
	rm -f *.o flux_os.bin
	