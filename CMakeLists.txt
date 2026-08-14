cmake_minimum_required(VERSION 3.8)
project(camera_driver)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
# CMAKE_CXX_STANDARD_REQUIREDをセットすることで、C++17がサポートされていないコンパイラでのビルドを防ぐことができる
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(CUDA REQUIRED)

if(CUDA_FOUND)
  message(STATUS "CUDA found: ${CUDA_VERSION_STRING}")
  CUDA_ADD_LIBRARY(cuda_library SHARED 
    include/cuda_kernel/cuda_resize.cu
    include/cuda_kernel/cuda_kernel.cpp)
  target_link_libraries(cuda_library 
    ${CUDA_LIBRARIES}
  )
else()
  message(FATAL_ERROR "CUDA not found")
endif()

add_library(camera_library SHARED include/camera_driver/camera_device.cpp)
target_link_libraries(camera_library 
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
  ${sensor_msgs_TARGETS}
  cuda_library
)
target_include_directories(camera_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

message(STATUS "camera_library include dir: ${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(main_node src/main_node.cpp)
target_link_libraries(main_node 
  camera_library
  rclcpp::rclcpp
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
