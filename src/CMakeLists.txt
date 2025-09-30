cmake_minimum_required(VERSION 3.10)
project(mini_ros2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include/mini_ros2)
find_package(Threads REQUIRED)

file(GLOB_RECURSE SRC_FILES *.cpp)

# 生成静态库
add_library(mini_ros2_lib STATIC ${SRC_FILES})
target_include_directories(mini_ros2_lib PUBLIC include/mini_ros2)
target_link_libraries(mini_ros2_lib PUBLIC rt Threads::Threads)

# 如果需要测试可执行文件，可以这样写
# add_executable(mini_ros2 ${SRC_FILES})
# target_link_libraries(mini_ros2 PRIVATE mini_ros2)