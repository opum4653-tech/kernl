cmake_minimum_required(VERSION 3.22.1)
project(ninmods-kernel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUSE_OPENGL)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/driver
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/font
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/ImGui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/Android_draw
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/Android_touch
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/native_surface
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/ImGui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/protect
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/func
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/other
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protect
    ${CMAKE_CURRENT_SOURCE_DIR}/src/offsets
)

if(NOT ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a")
endif()

# Prebuilt static libraries (curl, ssl, crypto)
add_library(curl STATIC IMPORTED)
set_target_properties(curl PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/curl/lib/${ANDROID_ABI}/libcurl.a)

add_library(ssl STATIC IMPORTED)
set_target_properties(ssl PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/openssl/lib/${ANDROID_ABI}/libssl.a)

add_library(crypto STATIC IMPORTED)
set_target_properties(crypto PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/network/openssl/lib/${ANDROID_ABI}/libcrypto.a)

# libevdev static submodule
add_library(libevdev STATIC
    jni/vendor/libevdev/source/libevdev/libevdev.c
    jni/vendor/libevdev/source/libevdev/libevdev-uinput.c
    jni/vendor/libevdev/source/libevdev/libevdev-names.c
)

file(GLOB_RECURSE SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/*.cpp
)

list(REMOVE_DUPLICATES SRC_FILES)
list(FILTER SRC_FILES EXCLUDE REGEX "imgui_demo\\.cpp")
list(FILTER SRC_FILES EXCLUDE REGEX "imgui_impl_vulkan\\.cpp")
list(FILTER SRC_FILES EXCLUDE REGEX "stb_image\\.cpp")

add_executable(ninmods-kernel ${SRC_FILES})

target_link_libraries(ninmods-kernel
    PRIVATE
    curl
    ssl
    crypto
    libevdev
    log
    android
    EGL
    GLESv3
    z
    dl
)

target_link_options(ninmods-kernel PRIVATE
    -Wl,--gc-sections
    -Wl,--strip-all
    -pie
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -Wl,-z,max-page-size=16384
)
