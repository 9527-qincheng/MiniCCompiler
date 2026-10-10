# ============================================================================
# MiniC 编译器 —— CMake 构建配置（Q2 定稿：C++17 + CMake）
# 使用 GLOB_RECURSE 自动收集 src 下源文件，产出 minic 可执行文件
# 构建：
#   cmake -S . -B build -G "MinGW Makefiles"
#   cmake --build build
#   或：cmake -S . -B build && cmake --build build --config Release
# ============================================================================
cmake_minimum_required(VERSION 3.10)

project(MiniCCompiler
        VERSION 0.1.0
        DESCRIPTION "MiniC compiler skeleton (challenge edition)"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 自动收集 src 下所有源文件（GLOB_RECURSE）
file(GLOB_RECURSE MINIC_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(minic ${MINIC_SOURCES})

# 头文件搜索路径
target_include_directories(minic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(minic PRIVATE /W3)
else()
    target_compile_options(minic PRIVATE -Wall -Wextra)
endif()
