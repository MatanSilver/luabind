cmake_minimum_required(VERSION 3.25)
project(luawrapper)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake) # Include Conan-generated file
conan_basic_setup(TARGETS) # Introduce Conan-generated targets

add_library(luawrapper luawrapper.cpp detail.hpp traits.hpp)
target_link_libraries(luawrapper CONAN_PKG::lua)

add_executable(tests tests.cpp)
target_link_libraries(tests luawrapper CONAN_PKG::gtest)