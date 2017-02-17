# --- START OF CONFIG ---------------------------------------------------
# Edit the following variables for your own needs

# toolchain configuration
PREFIX	?= arm-none-eabi
CC       = $(PREFIX)-gcc
LD       = $(PREFIX)-gcc
OBJCOPY  = $(PREFIX)-objcopy
OBJDUMP  = $(PREFIX)-objdump
GDB      = $(PREFIX)-gdb

OOCD = openocd
OOCD_CFG = stm32f4discovery.cfg

TOOLCHAIN_DIR ?= /opt/arm-none-eabi

# default build configuration
# "make BUILD=release" does a release build
BUILD:=debug

# basic build flags configuration
CFLAGS+=-Wall -std=c99 -pedantic -Wextra -Wimplicit-function-declaration \
        -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes \
        -Wundef -Wshadow \
        -fno-common -mcpu=cortex-m4 -mthumb \
        -mfloat-abi=hard -mfpu=fpv4-sp-d16 -MD -DSTM32F4

LDFLAGS+=--static -lm -lnosys -L$(TOOLCHAIN_DIR)/lib/thumb/cortex-m4/float-abi-hard/fpuv4-sp-d16 \
         -L$(TOOLCHAIN_DIR)/lib \
         -nostartfiles -Wl,--gc-sections \
         -mthumb -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16

# local include dir
CFLAGS+=-Iinclude

# the LD script
LDFLAGS+=-Tldscripts/stm32f4-discovery-$(BUILD).ld

# Flags for libopencm3
CM3_DIR ?= $(HOME)/code/c/stm32/libopencm3
CFLAGS+=-I$(CM3_DIR)/include
LDFLAGS+=-L$(CM3_DIR)/lib -lopencm3_stm32f4

# build type specific flags
CFLAGS_debug=-O0 -ggdb -DDEBUG
LDFLAGS_debug=

CFLAGS_release=-O3
LDFLAGS_release=

# target configuration
TARGET := stmusiclight
VERSION := 0.0.0
VCSVERSION := $(shell git rev-parse --short HEAD)

# source files for the project
SOURCE := $(shell find src/ -name '*.c')
INCLUDES := $(shell find src/ -name '*.h')

# additional dependencies for build (proper targets must be specified by user)
DEPS :=

# default target
all: $(TARGET)

# user-specific targets

# --- END OF CONFIG -----------------------------------------------------

OBJ1=$(patsubst %.c, %.o, $(SOURCE))
OBJ=$(patsubst src/%, obj/$(BUILD)/%, $(OBJ1))

VERSIONSTR="\"$(VERSION)-$(VCSVERSION)\""

CFLAGS+=-DVERSION=$(VERSIONSTR)

TARGET_BASE := bin/$(BUILD)/$(TARGET)

CFLAGS+=$(CFLAGS_$(BUILD))
LDFLAGS+=$(LDFLAGS_$(BUILD))

.PHONY show_cflags:
	@echo --- Build parameters:  ------------------------------------------
	@echo CFLAGS\=$(CFLAGS)
	@echo LDFLAGS\=$(LDFLAGS)
	@echo SOURCE\=$(SOURCE)
	@echo -----------------------------------------------------------------

$(TARGET): show_cflags $(TARGET_BASE).elf $(TARGET_BASE).hex \
	         $(TARGET_BASE).lss $(TARGET_BASE).bin
	@echo ">>> $(BUILD) build complete."

$(TARGET_BASE).elf: $(DEPS) $(OBJ) $(INCLUDES) Makefile
	@echo Linking $@ ...
	@mkdir -p $(shell dirname $@)
	@$(LD) -o $(TARGET_BASE).elf $(OBJ) $(LDFLAGS)

$(TARGET_BASE).hex: $(TARGET_BASE).elf
	@echo "Generating $@ ..."
	@$(OBJCOPY) -Oihex $< $@

$(TARGET_BASE).bin: $(TARGET_BASE).elf
	@echo "Generating $@ ..."
	@$(OBJCOPY) -Obinary $< $@

$(TARGET_BASE).lss: $(TARGET_BASE).elf
	@echo "Generating $@ ..."
	@$(OBJDUMP) -S $< > $@

obj/$(BUILD)/%.o: src/%.c $(INCLUDES) Makefile
	@echo "Compiling $< ..."
	@mkdir -p $(shell dirname $@)
	@$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET_BASE).elf
	rm -f $(TARGET_BASE).hex
	rm -f $(TARGET_BASE).lss
	rm -f $(TARGET_BASE).bin
	rm -f $(OBJ)

program: program_$(BUILD)

program_release: $(TARGET_BASE).hex
	$(OOCD) -f $(OOCD_CFG) \
		-c "init" \
		-c "reset halt" \
		-c "flash write_image erase $(TARGET_BASE).hex" \
		-c "reset" \
		-c "shutdown"

program_debug: $(TARGET_BASE).hex
	$(OOCD) -f $(OOCD_CFG) \
		-c "init" \
		-c "reset halt" \
		-c "load_image $(TARGET_BASE).hex" \
		-c "resume `arm-none-eabi-readelf -h $(TARGET_BASE).elf | awk '/Entry/{print $$4;}'`" \
		-c "shutdown"

debug: $(TARGET_BASE).hex
	$(OOCD) -f $(OOCD_CFG) \
		-c "init"
