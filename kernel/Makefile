LD := aarch64-linux-gnu-ld

AS := aarch64-linux-gnu-as
AFLAGS := -I.

CC := aarch64-linux-gnu-gcc
CFLAGS := -ffreestanding -I. -O0

CFLAGS_NO_INIT :=
CFLAGS_INIT := -fno-pic -g

C_SOURCES := $(shell find -path ./tests -prune -o -path ./arch -prune -o -name "**.c" -print) # include -print so that it doesn't print ./tests (man find(1))
ASM_SOURCES := $(shell find arch/aarch64 -name "**.s" -o -name "**.S" )
ASM_OBJECTS := $(patsubst %.s,%.o,$(filter %.s,$(ASM_SOURCES))) $(patsubst %.S,%.o,$(filter %.S,$(ASM_SOURCES))) 
PLTFRM_SOURCES := $(shell find arch/aarch64 -name "**.c")
INIT_SOURCES := $(shell find arch/aarch64/init/ init/ -name "**.c")
INIT_OBJECTS := $(INIT_SOURCES:%.c=%.o)

OBJECTS := $(ASM_OBJECTS) $(PLTFRM_SOURCES:%.c=%.o) $(C_SOURCES:%.c=%.o)

all: kernel.elf

clean:
	rm $(OBJECTS) kernel.elf

kernel.elf: $(OBJECTS) linker_script.ld
	$(info C_SOURCES is $(C_SOURCES))
	$(info ASM_SOURCES is $(ASM_SOURCES))
	$(info ASM_OBJECTS is $(ASM_OBJECTS))
	$(LD) -o $@ -T linker_script.ld $(OBJECTS)

$(INIT_OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $(CFLAGS_INIT) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) $(CFLAGS_NO_INIT) -o $@ $<

%.o: %.s
	$(AS) -c $(AFLAGS) -o $@ -mcpu=all $<
%.o: %.S
	$(AS) -c $(AFLAGS) -o $@ -mcpu=all $<
