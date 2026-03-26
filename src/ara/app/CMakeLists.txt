# src/ara/app/CMakeLists.txt
add_library(ara_app STATIC
    vehicle_signal_swc.cpp
)
target_include_directories(ara_app PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ara_app PUBLIC ara_com ara_log)

if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ara_app PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(ara_app PUBLIC --coverage)
endif()
