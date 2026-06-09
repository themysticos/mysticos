ASM = nasm
CC = gcc
LD = ld

ASMFLAGS = -f elf32
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -c -Wall
LDFLAGS = -m elf_i386 -T linker.ld

OBJS = boot.o kernel.o api.o

all: os.iso

os.iso: kernel.bin
	@echo "Creating bootable ISO..."
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'menuentry "MysticOS" {' >> iso/boot/grub/grub.cfg
	echo '    multiboot /boot/kernel.bin' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	
	grub-mkrescue -o os.iso iso/ 2>/dev/null || \
	xorriso -as mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o os.iso iso/

kernel.bin: $(OBJS)
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

boot.o: boot.asm
	@echo "Assembling bootloader..."
	$(ASM) $(ASMFLAGS) -o $@ $<

kernel.o: kernel.c api.h
	@echo "Compiling kernel..."
	$(CC) $(CFLAGS) -o $@ $<

api.o: api.asm
	@echo "Assembling API..."
	$(ASM) $(ASMFLAGS) -o $@ $<

run: os.iso
	@echo "Starting MysticOS in QEMU..."
	qemu-system-i386 -cdrom os.iso -m 128 -boot d

debug: os.iso
	@echo "Starting with debug port..."
	qemu-system-i386 -cdrom os.iso -m 128 -s -S

clean:
	rm -f *.o *.bin os.iso
	rm -rf iso/

.PHONY: all run debug clean