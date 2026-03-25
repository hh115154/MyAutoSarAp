# src/ara/com/CMakeLists.txt
add_library(ara_com STATIC someip_binding.cpp)
target_include_directories(ara_com PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ara_com PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(ara_com PUBLIC --coverage)
endif()
