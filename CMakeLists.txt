cmake_minimum_required(VERSION 3.9)
project(sf2hpp)

add_library(sf2hpp INTERFACE)
target_include_directories(sf2hpp INTERFACE .)

add_executable(example example.cpp)
target_link_libraries(example PUBLIC sf2hpp)

add_test(NAME run_example COMMAND example ${TEST_SF2_FILE})
