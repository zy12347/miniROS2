# rm -rf build
# mkdir build
# cd build
# cmake ..
# make -j 4

#!/bin/bash

# 默认构建类型为 Release
BUILD_TYPE="Release"

# 检查构建类型参数
if [ "$1" = "debug" ]; then
    BUILD_TYPE="Debug"
    echo "Building in Debug mode with symbols for GDB"
fi

# 清理现有的构建目录（如果存在）
if [ -d "build" ]; then
    echo "Cleaning existing build directory..."
    rm -rf build
fi

# 创建构建目录
mkdir -p build
cd build

# 运行 CMake 并设置适当的构建类型
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_CXX_FLAGS_DEBUG="-g -O0" -DCMAKE_C_FLAGS_DEBUG="-g -O0" ..

# 构建项目
make -j $(nproc)

# 提供使用说明
if [ "$BUILD_TYPE" = "Debug" ]; then
    echo "\nDebug build completed. You can now debug with GDB:"
    echo "gdb ./build/tests/test_shared_memory"
else
    echo "\nRelease build completed. For debugging, run: ./build.sh debug"
fi