LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := vosk
LOCAL_SRC_FILES := kaldi_$(TARGET_ARCH_ABI)/vosk/libvosk.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := main

# Add your application source files here...
LOCAL_SRC_FILES := \
    main.cpp \
    geometry.cpp \
    geometry.hpp \
    stb_vorbis.cpp \
    stb_vorbis.hpp \
    audio.cpp \
    audio.hpp \
    font.cpp \
    font.hpp \
    gl_helper.cpp \
    gl_helper.hpp \
    log.hpp \
	color_palette.hpp 
 
SDL_PATH := ../SDL  # SDL \

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include  # SDL

LOCAL_SHARED_LIBRARIES := SDL3 vosk

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv3 -lOpenSLES -llog -landroid 

include $(BUILD_SHARED_LIBRARY)
