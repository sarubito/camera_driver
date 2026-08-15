cmake_minimum_required(VERSION 3.8)
project(camera_driver LANGUAGES CXX CUDA)

# CUDAアーキテクチャを設定（空の場合のCMP0104警告を回避）
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES "native")
endif()

# CUDAアーキテクチャ処理に対して新しいポリシー(CMP0104)を優先適用
if(POLICY CMP0104)
  cmake_policy(SET CMP0104 NEW)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
# CMAKE_CXX_STANDARD_REQUIREDをセットすることで、C++17がサポートされていないコンパイラでのビルドを防ぐことができる
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0146 NEW)

# 依存パッケージを検索
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(OpenCV REQUIRED)
find_package(CUDAToolkit REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CUDAToolkit_INCLUDE_DIRS})

if(CUDAToolkit_FOUND)
  message(STATUS "CUDA検出: ${CUDAToolkit_VERSION}")
  add_library(cuda_library SHARED 
    include/cuda_kernel/cuda_resize.cu
    include/cuda_kernel/cuda_yuv422_to_rgba8.cu
    include/cuda_kernel/cuda_kernel.cpp)
  # `cuda_kernel.cpp` を CUDA フロントエンドでコンパイルし <<<>>> によるカーネル起動を有効化
  set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cuda_kernel/cuda_kernel.cpp
    PROPERTIES LANGUAGE CUDA)
  # デバイスコードの分離コンパイルを有効化（別の翻訳単位で定義されたカーネルを起動可能にする）
  set_target_properties(cuda_library PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
  # 共有ライブラリを位置独立コード（PIC）にする
  set_target_properties(cuda_library PROPERTIES POSITION_INDEPENDENT_CODE ON)
  # CUDA のホストスタブ生成（nvcc）による警告を抑制
  target_compile_options(cuda_library PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-w>
  )
  # CUDA ランタイム（cudart）をリンク。利用可能なインポート済みターゲットを優先
  if(TARGET CUDA::cudart)
    target_link_libraries(cuda_library PRIVATE CUDA::cudart)
  elseif(TARGET CUDAToolkit::cudart)
    target_link_libraries(cuda_library PRIVATE CUDAToolkit::cudart)
  else()
    # フォールバック: FindCUDAToolkit が提供する変数を試す
    if(DEFINED CUDAToolkit_LIBRARIES AND CUDAToolkit_LIBRARIES)
      target_link_libraries(cuda_library PRIVATE ${CUDAToolkit_LIBRARIES})
    else()
      # 最終手段: ライブラリ名で cudart を検索
      find_library(CUDART_LIBRARY cudart HINTS /usr/local/cuda/lib64 /usr/lib64 /usr/lib)
      if(CUDART_LIBRARY)
        target_link_libraries(cuda_library PRIVATE ${CUDART_LIBRARY})
      else()
        message(FATAL_ERROR "cuda_library にリンクする CUDA ランタイム (cudart) を見つけられませんでした")
      endif()
    endif()
  endif()
else()
  message(FATAL_ERROR "CUDA が見つかりませんでした")
endif()

add_library(camera_library SHARED 
  include/camera_driver/camera_device.cpp
  include/camera_driver/common/common.cpp)
target_link_libraries(camera_library 
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
  ${sensor_msgs_TARGETS}
  ${OpenCV_LIBRARIES}
  cuda_library
)
target_include_directories(camera_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

message(STATUS "camera_library のインクルードディレクトリ: ${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(main_node src/main_node.cpp)
target_link_libraries(main_node 
  camera_library
  rclcpp::rclcpp
)

install(TARGETS
  camera_library
  main_node
  DESTINATION lib/${PROJECT_NAME}
)

# param ディレクトリをパッケージの共有ディレクトリにインストール
install(DIRECTORY param
  DESTINATION share/${PROJECT_NAME}
)

# launch ディレクトリをパッケージの共有ディレクトリにインストール
install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # 以下の行は著作権表記をチェックするリンターをスキップします
  # すべてのソースに著作権とライセンスを追加したらこの行をコメントアウトしてください
  set(ament_cmake_copyright_FOUND TRUE)
  # 以下の行は cpplint をスキップします（git リポジトリ内でのみ動作します）
  # このパッケージが git リポジトリにあり、すべてのソースに著作権とライセンスが追加されている場合はこの行をコメントアウトしてください
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
