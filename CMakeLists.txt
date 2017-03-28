cmake_minimum_required(VERSION 3.6)
project(WordsCounter)

SET(CMAKE_CXX_FLAGS  "-pthread")

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp ${GCC_COVERAGE_LINK_FLAGS})
add_executable(WordsCounter ${SOURCE_FILES})