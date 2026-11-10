LOCAL_PATH := $(call my-dir)
ROOT_JNI_PATH := $(LOCAL_PATH)

include $(CLEAR_VARS)
LOCAL_MODULE := curl
LOCAL_SRC_FILES := include/external/curl-android-$(TARGET_ARCH_ABI)/lib/libcurl.a
LOCAL_EXPORT_C_INCLUDES := $(ROOT_JNI_PATH)/include/external/curl-android-$(TARGET_ARCH_ABI)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := ssl
LOCAL_SRC_FILES := include/external/openssl-android-$(TARGET_ARCH_ABI)/lib/libssl.a
LOCAL_EXPORT_C_INCLUDES := $(ROOT_JNI_PATH)/include/external/openssl-android-$(TARGET_ARCH_ABI)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := crypto
LOCAL_SRC_FILES := include/external/openssl-android-$(TARGET_ARCH_ABI)/lib/libcrypto.a
LOCAL_EXPORT_C_INCLUDES := $(ROOT_JNI_PATH)/include/external/openssl-android-$(TARGET_ARCH_ABI)/include
include $(PREBUILT_STATIC_LIBRARY)

# Include libevdev sub-module
include $(ROOT_JNI_PATH)/vendor/libevdev/Android.mk

# Restore ROOT_JNI_PATH
LOCAL_PATH := $(ROOT_JNI_PATH)

include $(CLEAR_VARS)

OPENGL_DRAW = 1

LOCAL_MODULE := ninmods-kernel

LOCAL_CFLAGS := -std=c++17
LOCAL_CFLAGS += -fvisibility=hidden
LOCAL_CFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR

LOCAL_CPPFLAGS := -std=c++17
LOCAL_CPPFLAGS += -fvisibility=hidden
LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_CPPFLAGS += -w -Wno-error=format-security -Werror
LOCAL_CPPFLAGS += -Wno-error=c++11-narrowing -fpermissive -Wall -fexceptions

ifeq ($(OPENGL_DRAW), 1)
   LOCAL_CFLAGS += -DUSE_OPENGL
   LOCAL_CPPFLAGS += -DUSE_OPENGL
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/src
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_vulkan
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui/backends
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/external/curl-android-$(TARGET_ARCH_ABI)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/external/openssl-android-$(TARGET_ARCH_ABI)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/external/nlohmann
LOCAL_C_INCLUDES += $(LOCAL_PATH)/vendor/glm
LOCAL_C_INCLUDES += $(LOCAL_PATH)/vendor/libevdev/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/vendor/libevdev/source/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/vendor/libevdev/source/libevdev
LOCAL_C_INCLUDES += $(LOCAL_PATH)/minitouch

LOCAL_SRC_FILES := \
    src/main.cpp \
    src/Android_draw/draw.cpp \
    src/Android_touch/TouchHelperA.cpp \
    src/ImGui/imgui.cpp \
    src/ImGui/imgui_demo.cpp \
    src/ImGui/imgui_draw.cpp \
    src/ImGui/imgui_tables.cpp \
    src/ImGui/imgui_widgets.cpp \
    src/ImGui/backends/imgui_impl_android.cpp \
    src/ENC/oxorany.cpp \
    src/display_manager.cpp \
    src/crash_handler.cpp \

ifeq ($(OPENGL_DRAW), 1)
    LOCAL_SRC_FILES += src/ImGui/backends/imgui_impl_opengl3.cpp
else
    LOCAL_SRC_FILES += src/ImGui/backends/imgui_impl_vulkan.cpp
    LOCAL_SRC_FILES += src/Android_vulkan/vulkan_wrapper.cpp
    LOCAL_SRC_FILES += src/Android_vulkan/VulkanUtils.cpp
endif

LOCAL_STATIC_LIBRARIES := curl ssl crypto libevdev

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3 -lz -ldl

include $(BUILD_EXECUTABLE)
