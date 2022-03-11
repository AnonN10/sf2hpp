cmake_minimum_required(VERSION 3.9)
project(sf2hpp)

set(CMAKE_CXX_STANDARD 17)

add_library(sf2hpp INTERFACE)
target_include_directories(sf2hpp INTERFACE .)

add_executable(example example.cpp)
target_link_libraries(example PUBLIC sf2hpp)

include(CTest)
set(TEST_SF2_FILE "UprightPianoKW-small-20190703.sf2")
add_test(NAME run_example COMMAND example "${PROJECT_SOURCE_DIR}/data/${TEST_SF2_FILE}")
