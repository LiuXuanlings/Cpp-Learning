cmake_minimum_required(VERSION 3.10)
project(MemoryPool)
set(CMAKE_CXX_STANDARD 11)

# 1. 全局配置：Debug模式自动加-g，Release模式加优化
# CMAKE_CXX_FLAGS_DEBUG 是Debug模式的默认编译选项，这里追加-g（其实CMake默认Debug已经带-g，加了更保险）
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
# Release模式加优化，对比用
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# 2. 定义源文件和头文件（修正文件名 StackAlloc.h）
set(SOURCE_FILES MemoryPool.tcc test.cpp)
set(HEADER_FILES MemoryPool.h StackAlloc.h)
add_executable(MemoryPool ${SOURCE_FILES} ${HEADER_FILES})

# 3. Debug模式下的额外配置（打印提示+定义DEBUG宏）
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Debug mode enabled: 添加-g调试符号 + 定义DEBUG宏")
    add_definitions(-DDEBUG)
endif ()

# 使用方式：
# mkdir build
# cd build
# cmake -DCMAKE_BUILD_TYPE=Debug ..
# make