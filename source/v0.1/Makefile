CC := clang
OBJCOPY := llvm-objcopy
CFLAGS = --target=armv7a-none-eabi -march=armv7-a -mcpu=cortex-a53 -marm -ffreestanding -nostdlib -fno-builtin -fno-pic -fno-pie -fno-stack-protector -O2 -Wall -Wextra
LDFLAGS = -fuse-ld=lld -nostdlib -Wl,-T,src/linker.ld -Wl,--build-id=none
.PHONY: all clean
all: xdos32.img
start.o: src/start.S
	$(CC) $(CFLAGS) -c -o $@ $<
kernel.o: src/kernel.c
	$(CC) $(CFLAGS) -c -o $@ $<
xdos32.elf: start.o kernel.o src/linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ start.o kernel.o
xdos32.img: xdos32.elf
	$(OBJCOPY) -O binary $< $@
clean:
	rm -f start.o kernel.o xdos32.elf xdos32.img
