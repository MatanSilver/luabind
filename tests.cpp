//
// Created by Matan Silver on 5/29/23.
//
#include "luawrapper.hpp"
#include "gtest/gtest.h"

TEST(LuaWrapper, Call) {
    luawrapper::Lua lua;
    lua.loadScript(R"(
            myprint = function(a, b)
                print(a, b)
            end
        )");
    lua.call("myprint", "thing", "stuff");
}

TEST(LuaWrapper, CallRNumberArg) {
    luawrapper::Lua lua;
    lua.loadScript(R"(
            myadd = function(a, b)
                return a+b
            end
        )");
    int x = lua.callr("myadd", 1, 2);
    ASSERT_EQ(x, 3);
}

TEST(LuaWrapper, CallRBooleanArg) {
    luawrapper::Lua lua;
    lua.loadScript(R"(
            myand = function(a, b)
                return a and b
            end
        )");
    bool x = lua.callr("myand", true, false);
    ASSERT_EQ(x, false);

    bool y = lua.callr("myand", true, true);
    ASSERT_EQ(y, true);
}

TEST(LuaWrapper, CallRStringArg) {
    luawrapper::Lua lua;
    lua.loadScript(R"(
            identity = function(a)
                return a
            end
        )");
    std::string x = lua.callr("identity", std::string("thing"));
    ASSERT_EQ(x, std::string{"thing"});
}

TEST(LuaWrapper, CallRCharArg) {
    luawrapper::Lua lua;
    lua.loadScript(R"(
            identity = function(a)
                return a
            end
        )");
    std::string x = lua.callr("identity", "thing");
    ASSERT_EQ(x, std::string{"thing"});
}

int thing(int, int) {
    return 0;
}

TEST(LuaWrapper, Expose) {
    auto timesTwo = [](int x)->int {
        return x*2;
    };
    luawrapper::Lua lua;
    auto timesTwoCFun = luawrapper::detail::adapt<0>(timesTwo);
    lua.expose("timesTwo", timesTwoCFun);
    lua.loadScript(R"(
            callIntoCFunc = function(a)
                return timesTwo(a)
            end
        )");
    int x = lua.callr("callIntoCFunc", 4);
    ASSERT_EQ(x, 8);

}

TEST(LuaWrapper, IncorrectArgumentType) {
    auto timesTwo = [](int x)->int {
        return x*2;
    };
    luawrapper::Lua lua;
    auto timesTwoCFun = luawrapper::detail::adapt<0>(timesTwo);
    lua.expose("timesTwo", timesTwoCFun);
    auto willThrow = [&](){ int x = lua.callr("timesTwo", "thing"); };
    ASSERT_THROW(willThrow(), luawrapper::detail::IncorrectType);
}

TEST(LuaWrapper, IncorrectReturnType) {
    luawrapper::Lua lua;
    lua.loadScript(R"(
            identity = function(a)
                return a
            end
        )");
    auto willThrow = [&](){ std::string x = lua.callr("identity", 1); };
    ASSERT_THROW(willThrow(), luawrapper::detail::IncorrectType);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}