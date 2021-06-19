cmake_minimum_required(VERSION 3.19)
project(xor-crypto-token)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ltbb")

add_executable(xor-crypto-token main.cpp)