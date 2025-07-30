LD := aarch64-linux-gnu-ld

AS := aarch64-linux-gnu-as
AFLAGS := -g

CC := aarch64-linux-gnu-gcc
CFLAGS := -ffreestanding -g -I.

C_SOURCES := $(shell find -path ./tests -prune -o -path ./arch -prune -o -name "**.c" -print) # include -print so that it doesn't print ./tests (man find(1))
ASM_SOURCES := $(shell find arch/aarch64 -name "**.s" -o -name "**.S" )
PLTFRM_SOURCES := $(shell find arch/aarch64 -name "**.c")
OBJECTS := $(ASM_SOURCES:%.s=%.o) $(PLTFRM_SOURCES:%.c=%.o) $(C_SOURCES:%.c=%.o)

all: kernel.elf

clean:
	rm $(OBJECTS) kernel.elf

kernel.elf: $(OBJECTS) linker_script.ld
	$(info C_SOURCES is $(C_SOURCES))
	$(LD) -o $@ -T linker_script.ld $(OBJECTS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.s
	$(AS) -c $(AFLAGS) -o $@ -mcpu=all $<
