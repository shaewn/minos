LD := aarch64-linux-gnu-ld

AS := aarch64-linux-gnu-as
AFLAGS := -g

CC := aarch64-linux-gnu-gcc
CFLAGS := -ffreestanding -g

C_SOURCES := $(shell find -path ./tests -prune -o -name "**.c" -print)
ASM_SOURCES := $(shell find arch/aarch64 -name "**.s" )
OBJECTS := $(ASM_SOURCES:%.s=%.o) $(C_SOURCES:%.c=%.o)

all: kernel.elf

clean:
	rm $(OBJECTS)

kernel.elf: $(OBJECTS) linker_script.ld
	$(info C_SOURCES is $(C_SOURCES))
	$(LD) -o $@ -T linker_script.ld $(OBJECTS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.s
	$(AS) -c $(AFLAGS) -o $@ -mcpu=all $<
