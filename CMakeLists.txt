cmake_minimum_required(VERSION 3.25)
project(luabind)

set(CMAKE_CXX_STANDARD 20)

MESSAGE(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")

find_package(GTest CONFIG REQUIRED)
find_package(Lua REQUIRED)

add_library(luabind src/luabind.cpp)
target_include_directories(luabind PRIVATE ${LUA_INCLUDE_DIR})
target_include_directories(luabind PUBLIC src)
target_link_libraries(luabind PRIVATE ${LUA_LIBRARIES})

add_executable(tests test/tests.cpp)
target_link_libraries(tests PRIVATE luabind ${LUA_LIBRARIES} GTest::gtest GTest::gtest_main)
target_include_directories(tests PUBLIC src)
target_include_directories(tests PRIVATE ${LUA_INCLUDE_DIR})
target_include_directories(tests PUBLIC src)
