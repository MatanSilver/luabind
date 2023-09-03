cmake_minimum_required(VERSION 3.25)
project(luabind)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake) # Include Conan-generated file
conan_basic_setup(TARGETS) # Introduce Conan-generated targets

add_library(luabind src/luabind.cpp)
target_link_libraries(luabind CONAN_PKG::lua)
target_include_directories(luabind PUBLIC src)

add_executable(tests test/tests.cpp)
target_link_libraries(tests luabind CONAN_PKG::gtest)
target_include_directories(tests PUBLIC src)