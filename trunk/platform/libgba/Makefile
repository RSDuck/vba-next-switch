ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring windows32,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
endif
endif

ifeq ($(platform), unix)
   TARGET := libsnes.so
   fpic := -fPIC
   SHARED := -shared
else ifeq ($(platform), osx)
   TARGET := libsnes.dylib
   fpic := -fPIC
   SHARED := -dynamiclib
else
   TARGET := vbanext-git-x86_64.dll
   CC = gcc
   CXX = g++
   SHARED := -shared
endif

VBA_DIR := ../../src/vba

VBA_SRC_DIRS := $(VBA_DIR) $(VBA_DIR)/gba $(VBA_DIR)/apu $(VBA_DIR)/gb $(VBA_DIR)/common

VBA_CXXSRCS := $(foreach dir,$(VBA_SRC_DIRS),$(wildcard $(dir)/*.cpp))
VBA_CXXOBJ := $(VBA_CXXSRCS:.cpp=.o)
VBA_CSRCS := $(foreach dir,$(VBA_SRC_DIRS),$(wildcard $(dir)/*.c))
VBA_COBJ := $(VBA_CSRCS:.c=.o)

OBJS := $(VBA_COBJ) $(VBA_CXXOBJ) libsnes.o

VBA_DEFINES := -D__LIBGBA__

CFLAGS += -O3 -g -std=gnu99 -fPIC $(VBA_DEFINES)
CXXFLAGS += -O3 -g -fPIC $(VBA_DEFINES)
LDFLAGS += -Wl,-no-undefined

INCDIRS := -I$(VBA_DIR)
LIBS := -lz

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(SHARED) $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCDIRS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(INCDIRS)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean
