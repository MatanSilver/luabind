cmake_minimum_required(VERSION 3.24)
project(luabind)

MESSAGE(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")

add_library(luabind INTERFACE)
target_include_directories(luabind INTERFACE src)
