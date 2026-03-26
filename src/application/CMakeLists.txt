# src/application/CMakeLists.txt

add_executable(MyAutoSarAp main.cpp)

target_link_libraries(MyAutoSarAp
    PRIVATE
        ara_exec
        ara_log
        ara_com
        ara_app
        ara_diag
        ara_per
)

target_include_directories(MyAutoSarAp
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
