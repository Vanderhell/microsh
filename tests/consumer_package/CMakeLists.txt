cmake_minimum_required(VERSION 3.21)

project(microsh_package_consumer LANGUAGES C CXX)

find_package(microsh CONFIG REQUIRED)

add_executable(package_consumer_c consumer_c.c)
target_link_libraries(package_consumer_c PRIVATE microsh::microsh)

add_executable(package_consumer_cpp consumer_cpp.cpp)
target_link_libraries(package_consumer_cpp PRIVATE microsh::microsh)
target_compile_features(package_consumer_cpp PRIVATE cxx_std_11)
