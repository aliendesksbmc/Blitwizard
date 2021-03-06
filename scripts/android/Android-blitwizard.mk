LOCAL_PATH := $(call my-dir)

APP_ABI := armeabi-v7a

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include $(LOCAL_PATH)/../vorbis/include $(LOCAL_PATH)/../ogg/include $(LOCAL_PATH)/../imgloader/ $(LOCAL_PATH)/../box2d/ $(LOCAL_PATH)/../lua/ $(LOCAL_PATH)/speex/

# Add your application source files here...
LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.cpp \
    SOURCEFILELIST

LOCAL_STATIC_LIBRARIES := SDL2 imgloader png zlib vorbis ogg box2d lua

LOCAL_CFLAGS := -O2 -s -pthread -DVERSION=VERSIONINSERT SPEEXFLAGSINSERT
LOCAL_LDLIBS := -lGLESv1_CM -llog -ldl -lGLESv2

include $(BUILD_SHARED_LIBRARY)
