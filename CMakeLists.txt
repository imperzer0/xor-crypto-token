cmake_minimum_required(VERSION 3.19)
project(xor-crypto-token)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ltbb -lprocps")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -lprocps")

add_executable(xor-crypto-token main.cpp)