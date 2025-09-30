cmake_minimum_required(VERSION 3.10)
project(mini_ros2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# 添加子目录（按模块划分）
add_subdirectory(src)        # 核心库
add_subdirectory(cli)        # 命令行工具
add_subdirectory(tests)      # 单元测试
add_subdirectory(examples)   # 示例程序（可选，可通过 option 控制）
# add_subdirectory(third_party)# 第三方依赖（如需要编译的库）