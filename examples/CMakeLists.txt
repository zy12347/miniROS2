cmake_minimum_required(VERSION 3.10)
project(mini_ros2_examples)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 示例程序
add_executable(talker_example basic/talker_example.cpp)
target_link_libraries(talker_example mini_ros2_lib pthread rt)

add_executable(listener_example basic/listener_example.cpp)
target_link_libraries(listener_example mini_ros2_lib pthread rt)

add_executable(service_example basic/service_example.cpp)
target_link_libraries(service_example mini_ros2_lib pthread rt)