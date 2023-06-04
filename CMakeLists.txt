cmake_minimum_required(VERSION 3.25)
project(luawrapper)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake) # Include Conan-generated file
conan_basic_setup(TARGETS) # Introduce Conan-generated targets

add_library(luawrapper src/luawrapper.cpp)
target_link_libraries(luawrapper CONAN_PKG::lua)
target_include_directories(luawrapper PUBLIC src)

add_executable(tests test/tests.cpp)
target_link_libraries(tests luawrapper CONAN_PKG::gtest)
target_include_directories(tests PUBLIC src)