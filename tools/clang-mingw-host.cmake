# clang-mingw-host.cmake: toolchain for llama.cpp's vulkan-shaders-gen (a helper run only while building), so it
# is built with our clang for the MinGW target like everything else. Used by tools\buildllama.cmd vulkan.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET x86_64-w64-windows-gnu)
set(CMAKE_CXX_COMPILER_TARGET x86_64-w64-windows-gnu)
set(CMAKE_SYSROOT O:/appsnotonc/gcc/mingw64)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -static")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
