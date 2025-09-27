cmake_minimum_required(VERSION 3.10)
project(mini_ros2_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE CORE_SRC src/*.cpp)
add_library(mini_ros2_core STATIC ${CORE_SRC})
target_include_directories(mini_ros2_core PUBLIC include/mini_ros2)

add_executable(test_shared_memory test_shared_memory.cpp)
target_link_libraries(test_shared_memory 
  PRIVATE mini_ros2_core 
)

add_executable(test_service test_service.cpp)
target_link_libraries(test_service 
  PRIVATE mini_ros2_core 
)
add_executable(test_pubsub test_pubsub.cpp)
target_link_libraries(test_pubsub 
  PRIVATE mini_ros2_core 
)
add_executable(test_node test_node.cpp)
target_link_libraries(test_node 
  PRIVATE mini_ros2_core 
)
add_executable(test_command_adapter test_command_adapter.cpp)
target_link_libraries(test_command_adapter 
  PRIVATE mini_ros2_core 
)
# add_executable(mini_ros2_test_exec ${TEST_FILES})
# target_include_directories(mini_ros2_test_exec PRIVATE include/mini_ros2 tests)
# target_link_libraries(mini_ros2_test_exec mini_ros2_lib pthread)