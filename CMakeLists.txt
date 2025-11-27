cmake_minimum_required(VERSION 3.10)
project(mini_ros2)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译优化选项
# 如果没有指定 CMAKE_BUILD_TYPE，默认为 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Build type not specified, defaulting to Release")
endif()

# Release 模式使用 -O2 优化
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
# Debug 模式使用 -g 调试信息
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# 或者强制所有模式都使用 -O2（可选）
# add_compile_options(-O2)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX Flags Release: ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CXX Flags Debug: ${CMAKE_CXX_FLAGS_DEBUG}")

include_directories(include)

# 添加子目录（按模块划分）
add_subdirectory(src)        # 核心库
add_subdirectory(cli)        # 命令行工具
add_subdirectory(tests)      # 单元测试
add_subdirectory(examples)   # 示例程序（可选，可通过 option 控制）
# add_subdirectory(third_party)# 第三方依赖（如需要编译的库）