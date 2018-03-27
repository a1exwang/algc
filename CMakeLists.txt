cmake_minimum_required(VERSION 3.0)
project(algc)

add_compile_options(-pg -g -O0)

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

add_executable(
        pm_gc
        pm_gc.cpp
)
target_link_libraries(pm_gc pmemobj)
