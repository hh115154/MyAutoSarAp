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
option(BUILD_TESTS    "Build unit tests"                    OFF)
option(ENABLE_COVERAGE "Enable code coverage (Debug only)"  OFF)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 覆盖率汇总信息
if(ENABLE_COVERAGE)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "ENABLE_COVERAGE is set but CMAKE_BUILD_TYPE is not Debug — coverage data may be unreliable.")
    endif()
    message(STATUS "Code coverage enabled. Use scripts/run_coverage.sh to generate the HTML report.")
endif()

# ==============================================================
# 安装规则
# ==============================================================
install(DIRECTORY include/ DESTINATION include)
