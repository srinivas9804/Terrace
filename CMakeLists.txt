cmake_minimum_required(VERSION 2.8.12)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

project(Terrace VERSION 1.0
        DESCRIPTION "Implementing Terrace"
        LANGUAGES CXX)

add_library(TLX INTERFACE)
target_include_directories(TLX INTERFACE include/tlx)
target_include_directories(TLX INTERFACE include)
add_executable(graph src/graph.cpp)
target_link_libraries(graph TLX)
