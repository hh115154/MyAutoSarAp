# src/ara/com/CMakeLists.txt
# ara::com 通常由中间件厂商提供实现，此处为占位
add_library(ara_com INTERFACE)
target_include_directories(ara_com INTERFACE ${CMAKE_SOURCE_DIR}/include)
