add_executable(
        list_test
        list_test.cpp
        hash_table_test.cpp)
target_link_libraries(list_test algc pmemobj pmempool pmem)

add_executable(
        mark_test
        mark_test.cpp
        single_list.h
)
target_link_libraries(mark_test algc allog pmemobj pmempool pmem)

add_executable(
        sweep_test
        sweep_test.cpp
        single_list.h
)
target_link_libraries(sweep_test algc allog pmemobj pmempool pmem)


add_executable(
        hash_table_test
        hash_table_test.cpp
)
target_link_libraries(hash_table_test hash_table allog algc pmemobj)