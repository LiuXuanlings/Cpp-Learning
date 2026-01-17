#3.10支持C++11标准,3.16支持C++17标准，注意#是注释符号
cmake_minimum_required(VERSION 3.10)

#project，这是必须的，定义项目名称
project(MemoryPoolProject)

set(CMAKE_CXX_STANDARD 11)
#require表示不能降级，必须使用C++11标准，否则报错
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

#这里的GLOB是缩写，表示“全局搜索”，用于查找符合特定模式的文件。
#GLOB将搜索src目录下所有以.cpp结尾的文件，并将它们的路径存储在SOURCES变量中。
file(GLOB SRC_FILES "src/*.cpp")
file(GLOB TEST_FILES "tests/*.cpp")

#这里的PROJECT_NAME是CMake的内置变量，表示当前项目的名称。
add_executable(${PROJECT_NAME} ${SRC_FILES} ${TEST_FILES})

#添加编译选项，用法模式：
#target_compile_options(<target> [PRIVATE|PUBLIC|INTERFACE] <options>...)
#其中PRIVATE表示这些选项仅适用于当前目标，PUBLIC表示这些选项适用于当前目标及其依赖项，INTERFACE表示这些选项仅适用于依赖当前目标的其他目标。
target_compile_options(${PROJECT_NAME} PRIVATE -g -pthread)

#链接pthread库，因为compile_options中使用了-pthread选项，如果不链接pthread库，可能会导致链接错误。
target_link_libraries(${PROJECT_NAME} pthread)
