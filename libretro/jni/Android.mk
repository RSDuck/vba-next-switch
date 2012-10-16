LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libretro
LOCAL_SRC_FILES    = ../../src/gba.cpp ../../src/memory.cpp ../../src/sound.cpp ../../libretro/libretro.cpp
LOCAL_CFLAGS = -DINLINE=inline -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DSPEEDHAX -DLSB_FIRST -D__LIBRETRO__
LOCAL_C_INCLUDES = ../src

include $(BUILD_SHARED_LIBRARY)
