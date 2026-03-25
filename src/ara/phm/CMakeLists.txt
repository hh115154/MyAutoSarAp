# src/ara/phm/CMakeLists.txt
add_library(ara_phm STATIC phm.cpp)
target_include_directories(ara_phm PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ara_phm PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(ara_phm PUBLIC --coverage)
endif()
