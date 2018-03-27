add_executable(
        list_test
        list_test.cpp
)
target_link_libraries(list_test algc pmemobj pmempool pmem)

add_executable(
        mark_test
        mark_test.cpp
)
target_link_libraries(mark_test algc allog pmemobj pmempool pmem)
