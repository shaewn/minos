LD := aarch64-linux-gnu-ld
LD_FLAGS := --sysroot=/usr/aarch64-linux-gnu

AS := aarch64-linux-gnu-as
AFLAGS := -I.

CC := aarch64-linux-gnu-gcc
CFLAGS := -ffreestanding -I. -O0 -Wall -Werror

CFLAGS_NO_INIT := -O2
CFLAGS_INIT := -fno-pic

C_SOURCES := $(shell find -path ./tests -prune -o -path ./arch -prune -o -name "*.c" -print) # include -print so that it doesn't print ./tests (man find(1))
ASM_SOURCES := $(shell find arch/aarch64 -name "*.S")
ASM_OBJECTS := $(patsubst %.s,%.o,$(filter %.s,$(ASM_SOURCES))) $(patsubst %.S,%.o,$(filter %.S,$(ASM_SOURCES)))
PLTFRM_SOURCES := $(shell find arch/aarch64 -name "*.c")
INIT_SOURCES := $(shell find arch/aarch64/init/ init/ -name "*.c")
INIT_OBJECTS := $(INIT_SOURCES:%.c=%.o)

OBJECTS := $(ASM_OBJECTS) $(PLTFRM_SOURCES:%.c=%.o) $(C_SOURCES:%.c=%.o)

TEMP_FILE_LIST := .temp_file_list.txt

all: kernel.elf

clean:
	rm -f $(OBJECTS)
	@if [ -f $(TEMP_FILE_LIST) ]; then \
		xargs rm -f < $(TEMP_FILE_LIST); \
		rm -f $(TEMP_FILE_LIST); \
	fi

kernel.elf: $(OBJECTS) linker_script.ld
	$(LD) -o $@ -T linker_script.ld $(OBJECTS) $(LD_FLAGS)

$(INIT_OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $(CFLAGS_INIT) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) $(CFLAGS_NO_INIT) -o $@ $<

%.o: %.S
	$(CC) -c $(AFLAGS) -o $@ -Wa,-mcpu=all $<

linker_script.ld: linker_script.ld.S
	$(CC) -E -P -x c -o $@ $<
	@echo $@ >> $(TEMP_FILE_LIST)
