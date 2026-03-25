# src/ara/crypto/CMakeLists.txt
add_library(ara_crypto STATIC crypto.cpp)
target_include_directories(ara_crypto PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ara_crypto PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(ara_crypto PUBLIC --coverage)
endif()
