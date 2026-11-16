#!/bin/sh
set -e
echo "[*] Building ninmods-kernel using NDK..."
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk -j$(nproc 2>/dev/null || echo 4)
if [ -f "package_sh.sh" ]; then
    chmod +x package_sh.sh
    ./package_sh.sh
fi
echo "[+] Build complete."
