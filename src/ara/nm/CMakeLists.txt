# src/ara/nm/CMakeLists.txt
add_library(ara_nm STATIC nm.cpp)
target_include_directories(ara_nm PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ara_nm PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(ara_nm PUBLIC --coverage)
endif()
