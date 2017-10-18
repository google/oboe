cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -std=c++11")

# Specify directories which the compiler should look for headers
include_directories(
  include
  src)

file(GLOB_RECURSE oboe_sources src/*)

add_library(oboe STATIC ${oboe_sources})