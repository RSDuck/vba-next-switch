#options, set 1 to enable
CELL_DEBUG_CONSOLE		= 0
CELL_DEBUG_FPS			= 0
VBA_PS3_EXPERIMENTAL_ISEL	= 0
ORIGINAL_BRANCHES		= 0
MULTIMAN_SUPPORT		= 0
SDK_340				= 1

# specify build tools
CELL_BUILD_TOOLS	= GCC
#explicitly set some cell sdk defaults
CELL_SDK		?= /usr/local/cell
# CELL_GPU_TYPE (currently RSX is only one option)
CELL_GPU_TYPE		= RSX
#CELL_PSGL_VERSION is debug, dpm or opt
CELL_PSGL_VERSION	= opt

ifeq ($(shell uname), Linux)
	CELL_HOST_PATH	= $(CELL_SDK)/host-linux
else
	CELL_HOST_PATH	= $(CELL_SDK)/host-win32
endif


#Python binary - only useful for PSL1ght scripts
PYTHONBIN		= python2.7

# set path to important executables
MAKE_FSELF		= $(CELL_HOST_PATH)/bin/make_fself
MAKE_FSELF_NPDRM	= $(CELL_HOST_PATH)/bin/make_fself_npdrm
MAKE_PACKAGE_NPDRM	= $(CELL_HOST_PATH)/bin/make_package_npdrm

# Geohot CFW defines
MKSELF_GEOHOT		= make_self_npdrm
MKPKG_PSLIGHT		= buildtools/PS3Py/pkg.py
PKG_FINALIZE		= package_finalize

STRIP			= $(CELL_HOST_PATH)/ppu/bin/ppu-lv2-strip
C			   = $(CELL_HOST_PATH)/ppu/bin/ppu-lv2-gcc
CC			   = $(CELL_HOST_PATH)/ppu/bin/ppu-lv2-g++

# set path to all relevants directories
SRC_DIR			= src
CELL_FRAMEWORK_DIR	= src/cellframework
CELL_FRAMEWORK2_DIR	= src/cellframework2
VBA_API_DIR		= ../../src/vba
UTIL_DIR		= ../common/utils
BUILD_DIR		= objs


# build src lists and object file lists
VBA_CPPSRCS		+= $(SRC_DIR)/ps3video.cpp \
		$(SRC_DIR)/emu-ps3.cpp \
		$(VBA_API_DIR)/Util.cpp \
		$(VBA_API_DIR)/apu/Sound_Buffer.cpp \
		$(VBA_API_DIR)/gb/GB.cpp \
		$(VBA_API_DIR)/gb/gbCheats.cpp \
		$(VBA_API_DIR)/gb/gbDis.cpp \
		$(VBA_API_DIR)/gb/gbGfx.cpp \
		$(VBA_API_DIR)/gb/gbGlobals.cpp \
		$(VBA_API_DIR)/gb/gbMemory.cpp \
		$(VBA_API_DIR)/gb/gbPrinter.cpp \
		$(VBA_API_DIR)/gb/gbSGB.cpp \
		$(VBA_API_DIR)/gb/gbSound.cpp \
		$(VBA_API_DIR)/gba/GBA.cpp \
		$(VBA_API_DIR)/gba/Sound.cpp \
		$(VBA_API_DIR)/gba/EEprom.cpp \
		$(VBA_API_DIR)/gba/Flash.cpp \
		$(VBA_API_DIR)/gba/RTC.cpp \
		$(VBA_API_DIR)/gba/Sram.cpp \
		$(VBA_API_DIR)/gba/bios.cpp \
		$(CELL_FRAMEWORK_DIR)/fileio/FileBrowser.cpp \
		$(SRC_DIR)/menu.cpp

VBA_CPPOBJS 		= $(VBA_CPPSRCS:.cpp=.ppu.o)

VBA_CSRCS		+=  $(SRC_DIR)/ps3input.c \
		$(SRC_DIR)/conf/config_file.c \
		$(VBA_API_DIR)/common/memgzio.c \
		$(CELL_FRAMEWORK2_DIR)/audio/audioport.c \
		$(CELL_FRAMEWORK2_DIR)/audio/librsound.c \
		$(CELL_FRAMEWORK2_DIR)/audio/rsound.c \
		$(CELL_FRAMEWORK2_DIR)/input/pad_input.c \
		$(CELL_FRAMEWORK2_DIR)/input/mouse_input.c \
		$(CELL_FRAMEWORK2_DIR)/utility/oskutil.c \
		$(UTIL_DIR)/zlib/deflate.c \
		$(UTIL_DIR)/zlib/gzlib.c \
		$(UTIL_DIR)/zlib/uncompr.c \
		$(UTIL_DIR)/zlib/adler32.c \
		$(UTIL_DIR)/zlib/zutil.c \
		$(UTIL_DIR)/zlib/inffast.c \
		$(UTIL_DIR)/zlib/gzread.c \
		$(UTIL_DIR)/zlib/crc32.c \
		$(UTIL_DIR)/zlib/gzwrite.c \
		$(UTIL_DIR)/zlib/inflate.c \
		$(UTIL_DIR)/zlib/infback.c \
		$(UTIL_DIR)/zlib/trees.c \
		$(UTIL_DIR)/zlib/inftrees.c \
		$(UTIL_DIR)/zlib/gzclose.c \
		$(UTIL_DIR)/zlib/compress.c

VBA_COBJS 		= $(VBA_CSRCS:.c=.ppu.o)

PPU_CPPOBJS		= $(VBA_CPPOBJS:%=$(BUILD_DIR)/%) 
PPU_COBJS		= $(VBA_COBJS:%=$(BUILD_DIR)/%)

# Output file
PPU_TARGET		= vba.ppu.elf


PPU_CXXFLAGS		= -std=c++98 -W -Wall -Wuninitialized -Wcomment -Wchar-subscripts \
			-Wdeprecated-declarations -Wendif-labels -Wformat=2 -Wformat-extra-args \
			-Wimplicit -Wimport -Winline -Wmissing-braces -Wparentheses -Wpointer-arith \
			-Wredundant-decls -Wreturn-type -Wshadow -Wsign-compare -Wswitch \
			-Wunknown-pragmas -Wunused -Wwrite-strings -fno-rtti -fno-exceptions
PPU_CFLAGS		= -W -Wall -Wuninitialized -Wcomment -Wchar-subscripts \
			-Wdeprecated-declarations -Wendif-labels -Wformat=2 -Wformat-extra-args \
			-Wimplicit -Wimport -Winline -Wmissing-braces -Wparentheses -Wpointer-arith \
			-Wredundant-decls -Wreturn-type -Wshadow -Wsign-compare -Wswitch \
			-Wunknown-pragmas -Wunused -Wwrite-strings					

PPU_CXXFLAGS	+= -DBLARGG_BIG_ENDIAN=1 -DNDEBUG=1 -D__CELLOS_LV2__ -DBRANCHLESS_GBA_GFX -DBLIP_BUFFER_FAST=1 -DFASTER_SOUND_HACK_NON_SILENCE=1 -DGEKKO -DPSGL -DPATH_MAX=1024 -DFINAL_VERSION -D__ppc__ -DSPEEDHAX -D__POWERPC__ -DC_CORE -DWORDS_BIGENDIAN -DUSE_CACHE_PREFETCH=1
PPU_CFLAGS		+= -DBLARGG_BIG_ENDIAN=1 -DNDEBUG=1 -D__CELLOS_LV2__ -DBRANCHLESS_GBA_GFX -DBLIP_BUFFER_FAST=1 -DFASTER_SOUND_HACK_NON_SILENCE=1 -DGEKKO -DPSGL -DPATH_MAX=1024 -DFINAL_VERSION -D__ppc__ -DSPEEDHAX -D__POWERPC__ -DC_CORE -DWORDS_BIGENDIAN -DUSE_CACHE_PREFETCH=1

# include flags
PPU_CXXFLAGS		+= -I. -I$(VBA_API_DIR) -I.../common/utils/zlib
PPU_CFLAGS		+= -I. -I$(VBA_API_DIR) -I.../common/utils/zlib

# compiler optimizations
PPU_CXXFLAGS		+= -O3 \
			-ffast-math \
			--param inline-unit-growth=200 \
			--param large-function-growth=200 \
			--param max-inline-insns-single=450
					
PPU_CFLAGS		+= -O3 \
			-ffast-math \
			--param inline-unit-growth=200 \
			--param large-function-growth=200 \
			--param max-inline-insns-single=450

EMULATOR_VERSION = 1.03
					



#for 3.41, comment this out if you want to compile for 1.92
PPU_CFLAGS		+= -DPS3_SDK_3_41
PPU_CXXFLAGS		+= -DPS3_SDK_3_41

# VBA optimization flags

ifeq ($(LOOP_UNROLL_ARRAY_4),1)
PPU_CFLAGS     += -DLOOP_UNROLL_ARRAY_4=1
PPU_CXXFLAGS   += -DLOOP_UNROLL_ARRAY_4=1
endif

ifeq ($(ORIGINAL_BRANCHES),1)
PPU_CFLAGS     += -DORIGINAL_BRANCHES=1
PPU_CXXFLAGS   += -DORIGINAL_BRANCHES=1
endif

ifeq ($(CELL_DEBUG_CONSOLE),1)
PPU_CFLAGS     += -DCELL_DEBUG_CONSOLE
PPU_CXXFLAGS   += -DCELL_DEBUG_CONSOLE
L_CONTROL_CONSOLE_LDLIBS = -lcontrol_console_ppu
L_NET_CTL_LDLIBS = -lnetctl_stub
endif

ifeq ($(CELL_DEBUG_FPS),1)
PPU_CFLAGS     += -DCELL_DEBUG_FPS
PPU_CXXFLAGS   += -DCELL_DEBUG_FPS
endif

ifeq ($(VBA_PS3_EXPERIMENTAL_ISEL),1)
PPU_CFLAGS     += -DVBA_PS3_EXPERIMENTAL_ISEL=1
PPU_CXXFLAGS   += -DVBA_PS3_EXPERIMENTAL_ISEL=1
endif

ifeq ($(MULTIMAN_SUPPORT),1)
PPU_CFLAGS     += -DMULTIMAN_SUPPORT=1
PPU_CXXFLAGS   += -DMULTIMAN_SUPPORT=1
   ifeq ($(shell uname), Linux)
      MKFSELF_WC		= $(HOME)/bin/make_self_wc
   else
      MKFSELF_WC		= $(CELL_SDK)/../bin/make_self_wc
   endif
endif

ifeq ($(SDK_340),1)
L_SYSUTIL_SCREENSHOT = -lsysutil_screenshot_stub
else
L_SYSUTIL_SCREENSHOT =
endif

# VBA must define
PPU_CXXFLAGS		+= -Dlinux -DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H
PPU_CFLAGS		+= -Dlinux -DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H
PPU_CXXFLAGS		+= -D__need_timeval
PPU_CFLAGS		+= -D__need_timeval

#debug flags for stdio logging
PPU_CXXFLAGS		+= -g	 
PPU_CFLAGS		+= -g	 

# enable debug netlogger
#PPU_CXXFLAGS 	+= -DCELL_DEBUG -DPS3_DEBUG_IP=\"192.168.1.7\" -DPS3_DEBUG_PORT=9002
#PPU_CFLAGS		+= -DCELL_DEBUG -DPS3_DEBUG_IP=\"192.168.1.7\" -DPS3_DEBUG_PORT=9002

# default vba file libs
PPU_CXXVBAFLAGS 	= $(PPU_CXXFLAGS)
PPU_CVBAFLAGS 		= $(PPU_CFLAGS)

# libs, order matters!
PPU_LDLIBS		+= -L. -L$(CELL_SDK)/target/ppu/lib/PSGL/RSX/opt \
			-ldbgfont -lPSGL -lPSGLcgc -lcgc -lgcm_cmd -lgcm_sys_stub -lresc_stub \
			-lm -lio_stub -lfs_stub -lsysutil_stub -lsysutil_game_stub $(L_SYSUTIL_SCREENSHOT) $(L_CONTROL_CONSOLE_LDLIBS) -lpngdec_stub -ljpgdec_stub \
			-lsysmodule_stub -laudio_stub -lpthread -lnet_stub $(L_NET_CTL_LDLIBS)

# RULE: ALL
all: $(PPU_TARGET) $(PPU_TARGET:%.elf=%.self)

# RULE: All cpp files, can differentiate for vba dir
$(BUILD_DIR)/%.ppu.o: %.cpp 
	@mkdir -p $(@D)
	$(if $(findstring $@,$(VBA_CPPOBJS:%=$(BUILD_DIR)/%)),\
		$(CC) $(PPU_CXXVBAFLAGS) -o $@ -c $<,\
		$(CC) $(PPU_CXXFLAGS) -o $@ -c $<)


# RULE: All c files, can differentiate for vba dir		
$(BUILD_DIR)/%.ppu.o: %.c 
	@mkdir -p $(@D)
	$(if $(findstring $@,$(VBA_COBJS:%=$(BUILD_DIR)/%)),\
		$(C) $(PPU_CVBAFLAGS) -o $@ -c $<,\
		$(C) $(PPU_CFLAGS) -o $@ -c $<)		
	
	
# RULE: build the elf file	
$(PPU_TARGET): $(PPU_CPPOBJS) $(PPU_COBJS)
	$(CC) $(PPU_CXXFLAGS) $(PPU_LDLIBS) -o $(PPU_TARGET) $(PPU_CPPOBJS) $(PPU_COBJS)
	
	
# RULE: build the self file
$(PPU_TARGET:%.elf=%.self): $(PPU_TARGET)
	$(MAKE_FSELF) $(PPU_TARGET) $(PPU_TARGET:%.elf=%.self)
	
# RULE: generates the compiler and linker flags for profile-arcs
genProfile: set_genProfile_flags all

set_genProfile_flags: $(BUILD_DIR)/$(CELL_FRAMEWORK_DIR)/network-stdio/net_stdio.ppu.o
	touch ./src/VbaPs3.cpp		
	$(eval PPU_CXXFLAGS +=	-DPS3_PROFILING -DPS3_PROFILING_IP=\"192.168.1.101\" -DPS3_PROFILING_PORT=9001)
	$(eval PPU_CFLAGS 	+=	-DPS3_PROFILING -DPS3_PROFILING_IP=\"192.168.1.101\" -DPS3_PROFILING_PORT=9001)
	$(eval PPU_CXXVBAFLAGS = $(PPU_CXXFLAGS) -fprofile-generate -fprofile-arcs)
	$(eval PPU_CVBAFLAGS = $(PPU_CFLAGS) -fprofile-generate -fprofile-arcs)
	$(eval PPU_LDLIBS += -lgcov)
	$(eval PPU_LDLIBS += -Wl,-wrap,fopen -Wl,-wrap,fclose -Wl,-wrap,fwrite -Wl,-wrap,fread -Wl,-wrap,fseek -Wl,-wrap,ftell -Wl,-wrap,fflush -Wl,-wrap,rewind -Wl,-wrap,fgetpos -Wl,-wrap,fsetpos -Wl,-wrap,setbuf -Wl,-wrap,setvbuf -Wl,-wrap,ungetc -Wl,-wrap,feof)
	
	
# RULE: use a generated profile (gcda files)
useProfile: set_useProfile_flags all

set_useProfile_flags: 
	touch ./src/VbaPs3.cpp
	$(eval PPU_CXXVBAFLAGS = $(PPU_CXXFLAGS) -fprofile-use)
	$(eval PPU_CVBAFLAGS = $(PPU_CFLAGS) -fprofile-use)
	$(eval PPU_LDLIBS += -fprofile-use)
	

# RULE: build the package
pkg: $(PPU_TARGET)
	$(STRIP) $(PPU_TARGET) 
ifeq ($(MULTIMAN_SUPPORT),1)
	$(MKFSELF_WC) $(PPU_TARGET) pkg/USRDIR/RELOAD.SELF
else
	$(MAKE_FSELF_NPDRM) $(PPU_TARGET) pkg/USRDIR/EBOOT.BIN
endif
	$(MAKE_PACKAGE_NPDRM) pkg/package.conf pkg


# RULE: testing...
echo:
	echo $(PPU_CPPOBJS)
	echo $(BUILD_DIR)/$(CELL_FRAMEWORK_DIR)/network-stdio/net_stdio.ppu.o
	
# RULE - clean vba or ps3 or utils or all	
clean_vba:
	@rm $(VBA_CPPOBJS:%=$(BUILD_DIR)/%) -f
	@rm $(VBA_CPPOBJS:%.o=$(BUILD_DIR)/%.d) -f
	@rm $(VBA_COBJS:%=$(BUILD_DIR)/%) -f
	@rm $(VBA_COBJS:%.o=$(BUILD_DIR)/%.d) -f
	
clean: clean_vba
	@rm -rf objs common ../src
	@rm $(PPU_TARGET) $(PPU_TARGET:%.elf=%.self) -f

#massively reduced filesize using MKSELF_GEOHOT - use this for normal jailbreak builds
pkg-signed: $(PPU_TARGET) 
ifeq ($(MULTIMAN_SUPPORT),1)
	$(MKFSELF_WC) $(PPU_TARGET) pkg/USRDIR/RELOAD.SELF
else
	$(MKSELF_GEOHOT) $(PPU_TARGET) pkg/USRDIR/EBOOT.BIN VBAM900000
endif
	$(PYTHONBIN) $(MKPKG_PSLIGHT) --contentid IV0002-VBAM90000_00-SAMPLE0000000001 pkg/ vbanext-ps3-v$(EMULATOR_VERSION)-fw3.41.pkg

#use this to create a PKG for use with CFW 3.55
pkg-signed-cfw: $(PPU_TARGET)
ifeq ($(MULTIMAN_SUPPORT),1)
	$(MKFSELF_WC) $(PPU_TARGET) pkg/USRDIR/RELOAD.SELF
else
	$(MKSELF_GEOHOT) $(PPU_TARGET) pkg/USRDIR/EBOOT.BIN VBAM900000
endif
	$(PYTHONBIN) $(MKPKG_PSLIGHT) --contentid IV0002-VBAM90000_00-SAMPLE0000000001 pkg/ vbanext-ps3-v$(EMULATOR_VERSION)-cfw3.55.pkg
	$(PKG_FINALIZE) vbanext-ps3-v$(EMULATOR_VERSION)-cfw3.55.pkg
