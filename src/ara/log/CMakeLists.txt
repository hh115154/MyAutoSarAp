# src/ara/log/CMakeLists.txt
add_library(ara_log STATIC logger.cpp)
target_include_directories(ara_log PUBLIC ${CMAKE_SOURCE_DIR}/include)
