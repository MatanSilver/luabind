cmake_minimum_required(VERSION 3.24)
project(luabind_tests)

set(CMAKE_CXX_STANDARD 20)

MESSAGE(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")

find_package(GTest CONFIG REQUIRED)
find_package(Lua REQUIRED)

add_subdirectory(../ luabind_binary_dir)

add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE luabind ${LUA_LIBRARIES} GTest::gtest GTest::gtest_main)
target_include_directories(tests PUBLIC src)
target_include_directories(tests PRIVATE ${LUA_INCLUDE_DIR})