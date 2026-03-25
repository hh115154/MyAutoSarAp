# src/ara/iam/CMakeLists.txt
add_library(ara_iam STATIC iam.cpp)
target_include_directories(ara_iam PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ara_iam PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(ara_iam PUBLIC --coverage)
endif()
