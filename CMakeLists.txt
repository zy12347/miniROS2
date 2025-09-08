cmake_minimum_required(VERSION 3.10)
project(sensor_service)

file(GLOB_RECURSE SRC_LIST ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(sensor_service ${SRC_LIST})
target_include_directories(sensor_service PRIVATE ${CMAKE_SOURCE_DIR}/include)

# 补全: 添加编译选项
target_compile_features(sensor_service PRIVATE cxx_std_17)
set_target_properties(sensor_service PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# 补全: 添加链接库（如有需要，可根据实际情况修改）
# target_link_libraries(sensor_service PRIVATE some_library)

# 补全: 安装规则（可选）
install(TARGETS sensor_service DESTINATION bin)
