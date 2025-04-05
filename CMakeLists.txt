cmake_minimum_required(VERSION 3.10)
project(EventRepresentation)

# C++17 を使用
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ヘッダーファイルのディレクトリを追加
include_directories(${PROJECT_SOURCE_DIR}/include)

# OpenCV を検索
find_package(OpenCV REQUIRED)

# HDF5 の C++ ライブラリを検索
find_package(HDF5 REQUIRED COMPONENTS CXX)
if(HDF5_FOUND)
    include_directories(${HDF5_INCLUDE_DIRS})
endif()

# ソースファイルを指定して実行ファイルを作成
add_executable(event_representation 
    src/main.cpp 
    src/EventFrame.cpp 
    src/H5Reader.cpp 
    src/StackedHistogram.cpp
)

# ライブラリのリンク
target_link_libraries(event_representation ${OpenCV_LIBS} ${HDF5_CXX_LIBRARIES})
