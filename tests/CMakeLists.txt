add_executable(microsh_test_all test_all.c)
target_link_libraries(microsh_test_all PRIVATE microsh)
add_test(NAME microsh_test_all COMMAND microsh_test_all)

add_executable(microsh_consumer_c
    consumers/c_consumer.c
    consumers/abi_guard_ref.c
)
target_link_libraries(microsh_consumer_c PRIVATE microsh)

add_executable(microsh_consumer_cpp
    consumers/cpp_consumer.cpp
    consumers/abi_guard_ref.c
)
target_link_libraries(microsh_consumer_cpp PRIVATE microsh)
target_compile_features(microsh_consumer_cpp PRIVATE cxx_std_11)

add_executable(microsh_multi_tu_c
    consumers/c_consumer.c
    consumers/abi_guard_ref.c
)
target_link_libraries(microsh_multi_tu_c PRIVATE microsh)
