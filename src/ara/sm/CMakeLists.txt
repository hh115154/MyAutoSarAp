# src/ara/sm/CMakeLists.txt
add_library(ara_sm STATIC sm.cpp)
target_include_directories(ara_sm PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ara_sm PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(ara_sm PUBLIC --coverage)
endif()
