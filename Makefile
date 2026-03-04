#---------------------------------------------------------------------------------
# GBA Action RPG — devkitARM Makefile
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# devkitPro paths (hardcoded for Docker container)
#---------------------------------------------------------------------------------
export DEVKITPRO := /opt/devkitpro
export DEVKITARM := $(DEVKITPRO)/devkitARM
export PATH      := $(DEVKITARM)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

LIBTONC := $(DEVKITPRO)/libtonc

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
export TARGET    := game
export BUILD     := build
export SOURCES   := source source/engine source/game source/states include/mgba
export INCLUDES  := include
export DATA      :=
export GRAPHICS  :=
export AUDIO     := audio/music audio/sfx

#---------------------------------------------------------------------------------
# Game header info (used by gbafix via gba_rules)
#---------------------------------------------------------------------------------
export GAME_TITLE := GHSTPROT
export GAME_CODE  := BGPE
export MAKER_CODE := HB

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH := -mthumb -mthumb-interwork

CFLAGS   := -g -Wall -Wextra -Wpedantic -Wshadow \
            -Wdouble-promotion -Wformat=2 -Wconversion -Wvla \
            -fno-strict-aliasing -fomit-frame-pointer \
            -O2 -mcpu=arm7tdmi -mtune=arm7tdmi -flto $(ARCH)

CFLAGS   += $(INCLUDE) $(EXTRA_CFLAGS)

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS  := -g $(ARCH)
LDFLAGS   = -g $(ARCH) -flto -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# Libraries: Maxmod before libtonc (order matters)
#---------------------------------------------------------------------------------
LIBS := -lmm -ltonc

#---------------------------------------------------------------------------------
# Library search paths
#---------------------------------------------------------------------------------
LIBGBA  := $(DEVKITPRO)/libgba
LIBDIRS := $(LIBTONC) $(LIBGBA)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules or variables
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

include $(DEVKITARM)/gba_rules

#---------------------------------------------------------------------------------
# find source files
#---------------------------------------------------------------------------------
ifneq ($(strip $(AUDIO)),)
    export AUDIOFILES := $(foreach dir,$(AUDIO),$(foreach ext,*.xm *.it *.s3m *.mod *.wav,$(wildcard $(CURDIR)/$(dir)/$(ext))))
    ifneq ($(strip $(AUDIOFILES)),)
        BINFILES += soundbank.bin
    endif
endif

export OUTPUT := $(CURDIR)/$(TARGET)

export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                $(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
                $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES += $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)

export HFILES     := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE    := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                     $(foreach dir,$(LIBDIRS),-isystem $(dir)/include) \
                     -I$(CURDIR)/$(BUILD)

export LIBPATHS   := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean test

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
# test: build with HEADLESS_TEST defined, then run mgba-rom-test
#---------------------------------------------------------------------------------
test:
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@rm -f $(BUILD)/main.o $(BUILD)/main.d
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile EXTRA_CFLAGS=-DHEADLESS_TEST
	@mgba-rom-test -S 0x03 --log-level 15 $(CURDIR)/$(BUILD)/$(TARGET).gba; \
	 TEST_RC=$$?; \
	 rm -f $(BUILD)/main.o $(BUILD)/main.d; \
	 $(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile; \
	 exit $$TEST_RC

#---------------------------------------------------------------------------------
clean:
	@echo cleaning...
	@rm -rf $(BUILD)/*.o $(BUILD)/*.d $(BUILD)/*.elf $(BUILD)/*.gba $(BUILD)/*.map
	@rm -f $(BUILD)/soundbank.bin $(BUILD)/soundbank.h $(BUILD)/soundbank_bin.h
	@rm -f $(TARGET).gba $(TARGET).elf

#---------------------------------------------------------------------------------
else

# Include gba_rules here too so pattern rules (%.elf, %.gba) are available
include $(DEVKITARM)/gba_rules

# Cancel Make's built-in '%: %.o' implicit link rule.
# Without this, Make links soundbank.bin.o back into soundbank.bin (an ELF),
# overwriting the raw mmutil soundbank data and causing Maxmod divide-by-zero.
%: %.o

DEPENDS := $(OFILES:.o=.d)

# OUTPUT must be a simple path for pattern rules to match
OUTPUT := $(CURDIR)/$(TARGET)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).gba : $(OUTPUT).elf

$(OUTPUT).elf : $(OFILES)

#---------------------------------------------------------------------------------
# soundbank rule
# Cancel implicit link rule (gba_rules '%: %.o' overwrites soundbank.bin with ELF)
#---------------------------------------------------------------------------------
ifneq ($(strip $(AUDIOFILES)),)
soundbank.bin soundbank.h : $(AUDIOFILES)
	@mmutil $^ -osoundbank.bin -hsoundbank.h
else
soundbank.bin : ;
endif

#---------------------------------------------------------------------------------
# binary data to object rules
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h : %.bin
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
