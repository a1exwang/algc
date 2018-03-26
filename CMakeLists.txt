cmake_minimum_required(VERSION 3.0)
project(algc)


add_library(
        algc
        gc.cpp
        gc.h
)


add_executable(
        algc_test
        gc_test.cpp
)
target_link_libraries(
        algc_test
        algc
)

add_executable(
        algc_perf
        gc_perf.cpp
        tools.cpp
)
target_link_libraries(
        algc_perf
        algc
)
