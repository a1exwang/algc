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
        pmemobj pmempool pmem
)

add_executable(
        pm_gc
        pmgc.cpp
)
target_link_libraries(pm_gc pmemobj pmempool pmem)

add_executable(
        list_test
        list_test.cpp
)
target_link_libraries(list_test algc pmemobj pmempool pmem)

add_executable(
        mark_test
        mark_test.cpp
)
target_link_libraries(mark_test algc pmemobj pmempool pmem)
