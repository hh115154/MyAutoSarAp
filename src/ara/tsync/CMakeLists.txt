# src/ara/tsync/CMakeLists.txt
add_library(ara_tsync STATIC tsync.cpp)
target_include_directories(ara_tsync PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ara_tsync PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(ara_tsync PUBLIC --coverage)
endif()
