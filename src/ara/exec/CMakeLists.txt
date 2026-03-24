# src/ara/exec/CMakeLists.txt
add_library(ara_exec STATIC execution_client.cpp)
target_include_directories(ara_exec PUBLIC ${CMAKE_SOURCE_DIR}/include)
