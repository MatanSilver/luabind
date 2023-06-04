//
// Created by Matan Silver on 5/29/23.
//
#include "luawrapper.hpp"
#include "gtest/gtest.h"

TEST(LuaWrapper, Call) {
    luawrapper::Lua lua;
    lua <<R"(
            myprint = function(a, b)
                print(a, b)
            end
        )";
    lua["myprint"]("thing", "stuff");
}

TEST(LuaWrapper, CallRNumberArg) {
    luawrapper::Lua lua;
    lua <<R"(
            myadd = function(a, b)
                return a+b
            end
        )";
    int x = lua["myadd"](1, 2);
    ASSERT_EQ(x, 3);
}

TEST(LuaWrapper, CallRBooleanArg) {
    luawrapper::Lua lua;
    lua <<R"(
            myand = function(a, b)
                return a and b
            end
        )";
    bool x = lua["myand"](true, false);
    ASSERT_EQ(x, false);

    bool y = lua["myand"](true, true);
    ASSERT_EQ(y, true);
}

TEST(LuaWrapper, CallRStringArg) {
    luawrapper::Lua lua;
    lua <<R"(
            identity = function(a)
                return a
            end
        )";
    std::string x = lua["identity"](std::string("thing"));
    ASSERT_EQ(x, std::string{"thing"});
}

TEST(LuaWrapper, CallRCharArg) {
    luawrapper::Lua lua;
    lua <<R"(
            identity = function(a)
                return a
            end
        )";
    std::string x = lua["identity"](std::string("thing"));
    ASSERT_EQ(x, std::string{"thing"});
}

TEST(LuaWrapper, Expose) {
    luawrapper::Lua lua;
    lua["timesTwo"] = [](int x)->int {
        return x*2;
    };;
    lua <<R"(
            callIntoCFunc = function(a)
                return timesTwo(a)
            end
        )";
    int x = lua["callIntoCFunc"](4);
    ASSERT_EQ(x, 8);
}

TEST(LuaWrapper, MultipleExposeSameSignature) {
    luawrapper::Lua lua;
    lua["timesTwo"] = [](int x)->int {
        return x*2;
    };;
    lua["timesThree"] = [](int x)->int {
        return x*3;
    };;
    lua <<R"(
            callIntoCFunc = function(a)
                return timesTwo(a) + timesThree(a)
            end
        )";
    int x = lua["callIntoCFunc"](4);
    ASSERT_EQ(x, 20);

}

TEST(LuaWrapper, IncorrectArgumentType) {
    luawrapper::Lua lua;
    lua["timesTwo"] = [](int x)->int {
        return x*2;
    };;
    auto willThrow = [&](){ int x = lua["timesTwo"](std::string("thing")); };
    ASSERT_THROW(willThrow(), luawrapper::detail::IncorrectType);
}

TEST(LuaWrapper, IncorrectReturnType) {
    luawrapper::Lua lua;
    lua <<R"(
            identity = function(a)
                return a
            end
        )";
    auto willThrow = [&](){ std::string x = lua["identity"](1);};
    ASSERT_THROW(willThrow(), luawrapper::detail::IncorrectType);
}

TEST(LuaWrapper, GetGlobalValue) {
    luawrapper::Lua lua;
    lua <<R"(
            foo=4
        )";
    int foo = lua["foo"];
    ASSERT_EQ(foo, 4);
}

TEST(LuaWrapper, LambdaWithCapturedState) {
    luawrapper::Lua lua;
    int accumulator{0};
    lua["incrementAccumulator"] = [&accumulator](){ accumulator++; };

    ASSERT_EQ(accumulator, 0);
    lua["incrementAccumulator"]();
    ASSERT_EQ(accumulator, 1);
}

int cFunc(int, int) {
    return 4;
}

TEST(LuaWrapper, FcnPtr) {
    luawrapper::Lua lua;
    lua["cFunc"] = &cFunc;
    int res = lua["cFunc"](1, 1);
    ASSERT_EQ(res, 4);
}

TEST(LuaWrapper, ChainedScripts) {
    luawrapper::Lua lua;
    lua << "x = 1"
        << "y = 2"
        << R"(
            sumXY = function()
                return x + y
            end
        )";
    int res = lua["sumXY"]();
    ASSERT_EQ(res, 3);
}

TEST(LuaWrapper, ChainedScriptsAndCFuncs) {
    luawrapper::Lua lua;

    // Usability of chaining with these different operators is iffy
    // Maybe different operators could be used with better precedence
    // to make this more natural
    ((lua << "x = 1"
          << "y = 2")
          ["sumAB"] = [](int a, int b) {return a + b;})
          << R"(
              sumXY = function()
                  return sumAB(x,y)
              end
              )";
    ASSERT_EQ((int)lua["sumXY"](), 3);
}

TEST(LuaWrapper, IntVector) {
    luawrapper::Lua lua;
    std::vector initialVec{1, 3, 4, 5};
    lua["newvec"] = initialVec;
    std::vector<int> reconstructedVec = lua["newvec"];
    ASSERT_EQ(initialVec, reconstructedVec);
}

TEST(LuaWrapper, StringVector) {
    luawrapper::Lua lua;
    std::vector<std::string> initialVec{"thing", "stuff"};
    lua["newvec"] = initialVec;
    std::vector<std::string> reconstructedVec = lua["newvec"];
    ASSERT_EQ(initialVec, reconstructedVec);
}

TEST(LuaWrapper, Tuple) {
    luawrapper::Lua lua;
    // We can even have tuples containing tuples!
    using T = std::tuple<std::string,
                         int,
                         bool,
                         std::tuple<int, bool>>;
    T initialTuple{"thing", 1, true, {4, false}};
    lua["newtuple"] = initialTuple;
    T reconstructedTuple = lua["newtuple"];
    ASSERT_EQ(initialTuple, reconstructedTuple);
}

TEST(LuaWrapper, TupleFromLuaFunction) {
    luawrapper::Lua lua;
    using T = std::tuple<int, bool, std::string>;
    lua << R"(
        createTuple = function()
            return {1, false, "thing"}
        end
    )";
    T actual = lua["createTuple"]();
    T expected{1, false, "thing"};
    ASSERT_EQ(expected, actual);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}