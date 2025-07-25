LD := aarch64-linux-gnu-ld
AS := aarch64-linux-gnu-as -g
CC := aarch64-linux-gnu-gcc -g

C_SOURCES := $(shell find -name "**.c")
ASM_SOURCES := startup.s arch/aarch64/atomic.s
OBJECTS := $(ASM_SOURCES:%.s=%.o) $(C_SOURCES:%.c=%.o)

all: kernel.elf

clean:
	rm $(OBJECTS)

kernel.elf: $(OBJECTS) linker_script.ld
	$(LD) -o $@ -T linker_script.ld $(OBJECTS)

%.o: %.c
	$(CC) -c -o $@ $<

%.o: %.s
	$(AS) -c -o $@ -mcpu=all $<
