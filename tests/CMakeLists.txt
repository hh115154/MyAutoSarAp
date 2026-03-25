# tests/CMakeLists.txt
# AUTOSAR AP R25-11 — 单元测试 + 覆盖率配置
# GTest 通过 Homebrew 安装：arch -arm64 brew install googletest

cmake_minimum_required(VERSION 3.16)

# ==============================================================
# GoogleTest — 使用本机 Homebrew 安装版本（arm64 原生）
# ==============================================================
find_package(GTest REQUIRED
    HINTS /opt/homebrew/Cellar/googletest/1.17.0/lib/cmake/GTest
          /opt/homebrew/lib/cmake/GTest
)
message(STATUS "GTest found: ${GTest_VERSION} at ${GTEST_INCLUDE_DIRS}")

# ==============================================================
# 覆盖率编译选项（仅 Debug 模式生效）
# ==============================================================
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "[Coverage] Enabling --coverage flags")
endif()

# ==============================================================
# 辅助函数：注册一个单元测试可执行文件
# ==============================================================
function(add_ara_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})

    target_link_libraries(${TEST_NAME}
        PRIVATE
            GTest::gtest_main
            ara_log
            ara_exec
            ara_com
            ara_diag
            ara_per
    )

    target_include_directories(${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )

    # 覆盖率 flags
    if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${TEST_NAME} PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(${TEST_NAME} PRIVATE --coverage)
    endif()

    include(GoogleTest)
    gtest_discover_tests(${TEST_NAME})
endfunction()

# ==============================================================
# 注册测试用例
# ==============================================================
add_ara_test(test_ara_log  unit/test_ara_log.cpp)
add_ara_test(test_ara_exec unit/test_ara_exec.cpp)
add_ara_test(test_ara_com  unit/test_ara_com.cpp)
add_ara_test(test_ara_diag unit/test_ara_diag.cpp)
