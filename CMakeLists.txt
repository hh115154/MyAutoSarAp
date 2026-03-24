cmake_minimum_required(VERSION 3.16)

project(MyAutoSarAp
    VERSION 1.0.0
    DESCRIPTION "AutoSAR Adaptive Platform Application"
    LANGUAGES CXX
)

# ==============================================================
# C++ 标准（AutoSAR AP 要求 C++14 及以上）
# ==============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================
# 编译选项
# ==============================================================
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wno-unused-parameter
)

# Debug / Release 配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -DDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -DNDEBUG)
endif()

# ==============================================================
# 全局头文件路径
# ==============================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# ==============================================================
# 子模块
# ==============================================================
add_subdirectory(src)

# ==============================================================
# 单元测试（可选，需要 GTest）
# ==============================================================
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ==============================================================
# 安装规则
# ==============================================================
install(DIRECTORY include/ DESTINATION include)
