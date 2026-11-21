cmake_minimum_required(VERSION 3.22.1)

# Android STL & Platform Configuration (Parity with Application.mk - set before project())
if(NOT ANDROID_PLATFORM)
    set(ANDROID_PLATFORM 24)
endif()
if(NOT ANDROID_STL)
    set(ANDROID_STL c++_static)
endif()
set(CMAKE_ANDROID_STL_TYPE c++_static)

project(ninmods-kernel LANGUAGES C CXX)

# Enforce Release build configuration by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type: Release Debug" FORCE)
endif()

# Export compilation database for IDEs (clangd / VS Code / CLion)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C & C++ Standards configuration
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Position Independent Code (PIE / PIC)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Target ABI Resolution & Validation
if(NOT ANDROID_ABI)
    if(CMAKE_ANDROID_ARCH_ABI)
        set(ANDROID_ABI ${CMAKE_ANDROID_ARCH_ABI})
    elseif(TARGET_ARCH_ABI)
        set(ANDROID_ABI ${TARGET_ARCH_ABI})
    else()
        set(ANDROID_ABI "arm64-v8a")
    endif()
endif()

# Verify prebuilt library existence for resolved ABI
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/includes/network/curl/lib/${ANDROID_ABI}/libcurl.a")
    message(FATAL_ERROR "Target ABI '${ANDROID_ABI}' is unsupported! Prebuilt network libraries exist only for arm64-v8a and armeabi-v7a.")
endif()

# Global preprocessor definitions
add_compile_definitions(USE_OPENGL)

# Comprehensive include paths with 100% Android.mk parity
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/driver
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/ImGui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/Android_draw
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/Android_touch
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/native_surface
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/Android_draw
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/Android_touch
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/ImGui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/network
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/curl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/openssl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/json
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/vendor/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/vendor/libevdev/include
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/vendor/libevdev/source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/vendor/libevdev/source/libevdev
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/minitouch
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FONTS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/func
    ${CMAKE_CURRENT_SOURCE_DIR}/src/func/aim
    ${CMAKE_CURRENT_SOURCE_DIR}/src/func/esp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/other
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protect
    ${CMAKE_CURRENT_SOURCE_DIR}/src/offsets
)

# Prebuilt static libraries (curl, ssl, crypto) with dependency topology
add_library(curl STATIC IMPORTED)
set_target_properties(curl PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/curl/lib/${ANDROID_ABI}/libcurl.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/curl/include
    INTERFACE_LINK_LIBRARIES "ssl;crypto;z"
)

add_library(ssl STATIC IMPORTED)
set_target_properties(ssl PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/openssl/lib/${ANDROID_ABI}/libssl.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/openssl/include
    INTERFACE_LINK_LIBRARIES "crypto"
)

add_library(crypto STATIC IMPORTED)
set_target_properties(crypto PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/openssl/lib/${ANDROID_ABI}/libcrypto.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/openssl/include
    INTERFACE_LINK_LIBRARIES "dl;z"
)

# libevdev static submodule (Strict parity with jni/vendor/libevdev/Android.mk)
add_library(libevdev STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/vendor/libevdev/source/libevdev/libevdev.c
)
target_include_directories(libevdev PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/vendor/libevdev/include
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/vendor/libevdev/source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/vendor/libevdev/source/libevdev
)
target_compile_options(libevdev PRIVATE
    -Oz
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -fomit-frame-pointer
    -fmerge-all-constants
    -fno-ident
)

# Auto-collect source files (.cpp) matching Android.mk with CONFIGURE_DEPENDS
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/*.cpp
)

list(REMOVE_DUPLICATES SRC_FILES)
list(FILTER SRC_FILES EXCLUDE REGEX "imgui_demo\\.cpp")
list(FILTER SRC_FILES EXCLUDE REGEX "imgui_impl_vulkan\\.cpp")
list(FILTER SRC_FILES EXCLUDE REGEX "stb_image\\.cpp")

add_executable(ninmods-kernel ${SRC_FILES})
set_target_properties(ninmods-kernel PROPERTIES
    OUTPUT_NAME "ninmods-kernel"
)

# Compiler flags matching jni/Android.mk and jni/Application.mk
target_compile_options(ninmods-kernel PRIVATE
    -std=c++17
    -fexceptions
    -fno-rtti
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -Oz
    -ffunction-sections
    -fdata-sections
    -fomit-frame-pointer
    -Wno-error=format-security
    -fno-color-diagnostics
    -fmerge-all-constants
    -fno-ident
)

# Linked libraries matching jni/Android.mk LOCAL_STATIC_LIBRARIES and LOCAL_LDLIBS
# (Curl -> SSL -> Crypto dependency topology, zero unneeded DT_NEEDED)
target_link_libraries(ninmods-kernel
    PRIVATE
    curl
    ssl
    crypto
    libevdev
    log
    EGL
    GLESv3
    z
    dl
)

# Linker flags matching jni/Android.mk
target_link_options(ninmods-kernel PRIVATE
    -static-libstdc++
    -Wl,--gc-sections
    -Wl,--strip-all
    -Wl,--build-id=none
    -Wl,--no-undefined
    -pie
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -Wl,-z,max-page-size=16384
)
