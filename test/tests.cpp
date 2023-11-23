//
// Created by Matan Silver on 5/29/23.
//
#include "luabind/luabind.hpp"
#include "gtest/gtest.h"

TEST(LuaBind, Call) {
    luabind::Lua lua;
    lua <<R"(
            myprint = function(a, b)
                print(a, b)
            end
        )";
    lua["myprint"]("thing", "stuff");
}

TEST(LuaBind, CallRNumberArg) {
    luabind::Lua lua;
    lua <<R"(
            myadd = function(a, b)
                return a+b
            end
        )";
    int x = lua["myadd"](1, 2);
    ASSERT_EQ(x, 3);
}

TEST(LuaBind, CallRBooleanArg) {
    luabind::Lua lua;
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

TEST(LuaBind, CallRStringArg) {
    luabind::Lua lua;
    lua <<R"(
            identity = function(a)
                return a
            end
        )";
    std::string x = lua["identity"](std::string("thing"));
    ASSERT_EQ(x, std::string{"thing"});
}

TEST(LuaBind, CallRCharArg) {
    luabind::Lua lua;
    lua <<R"(
            identity = function(a)
                return a
            end
        )";
    std::string x = lua["identity"](std::string("thing"));
    ASSERT_EQ(x, std::string{"thing"});
}

TEST(LuaBind, Expose) {
    luabind::Lua lua;
    lua["timesTwo"] = [](int x)->int {
        return x*2;
    };
    lua <<R"(
            callIntoCFunc = function(a)
                return timesTwo(a)
            end
        )";
    int x = lua["callIntoCFunc"](4);
    ASSERT_EQ(x, 8);
}

TEST(LuaBind, MultipleExposeSameSignature) {
    luabind::Lua lua;
    lua["timesTwo"] = [](int x)->int {
        return x*2;
    };
    lua["timesThree"] = [](int x)->int {
        return x*3;
    };
    lua <<R"(
            callIntoCFunc = function(a)
                return timesTwo(a) + timesThree(a)
            end
        )";
    int x = lua["callIntoCFunc"](4);
    ASSERT_EQ(x, 20);

}

TEST(LuaBind, IncorrectArgumentType) {
    luabind::Lua lua;
    lua["timesTwo"] = [](int x)->int {
        return x*2;
    };
    auto willThrow = [&](){ [[maybe_unused]] int x = lua["timesTwo"](std::string("thing")); };
    ASSERT_THROW(willThrow(), luabind::detail::IncorrectType);
}

TEST(LuaBind, IncorrectReturnType) {
    luabind::Lua lua;
    lua <<R"(
            identity = function(a)
                return a
            end
        )";
    auto willThrow = [&](){ std::string x = lua["identity"](1); };
    ASSERT_THROW(willThrow(), luabind::detail::IncorrectType);
}

TEST(LuaBind, IncorrectTypeBool) {
    luabind::Lua lua;
    lua <<R"(
        isNotBool = "thing"
    )";
    auto willThrow = [&]() { bool x = lua["isNotBool"]; };
    ASSERT_THROW(willThrow(), luabind::detail::IncorrectType);
}

TEST(LuaBind, IncorrectTypeVector) {
    luabind::Lua lua;
    lua <<R"(
        isNotVector = "thing"
    )";
    auto willThrow = [&]() { std::vector<int> x = lua["isNotVector"]; };
    ASSERT_THROW(willThrow(), luabind::detail::IncorrectType);
}

TEST(LuaBind, GetGlobalValue) {
    luabind::Lua lua;
    lua <<R"(
            foo=4
        )";
    int foo = lua["foo"];
    ASSERT_EQ(foo, 4);
}

TEST(LuaBind, LambdaWithCapturedState) {
    luabind::Lua lua;
    int accumulator{0};
    lua["incrementAccumulator"] = [&accumulator](){ accumulator++; };

    ASSERT_EQ(accumulator, 0);
    lua["incrementAccumulator"]();
    ASSERT_EQ(accumulator, 1);
}

int cFunc(int, int) {
    return 4;
}

TEST(LuaBind, FcnPtr) {
    luabind::Lua lua;
    lua["cFunc"] = &cFunc;
    int res = lua["cFunc"](1, 1);
    ASSERT_EQ(res, 4);
}

TEST(LuaBind, ChainedScripts) {
    luabind::Lua lua;
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

TEST(LuaBind, ChainedScriptsAndCFuncs) {
    luabind::Lua lua;

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

TEST(LuaBind, IntVector) {
    luabind::Lua lua;
    std::vector initialVec{1, 3, 4, 5};
    lua["newvec"] = initialVec;
    std::vector<int> reconstructedVec = lua["newvec"];
    ASSERT_EQ(initialVec, reconstructedVec);
}

TEST(LuaBind, StringVector) {
    luabind::Lua lua;
    std::vector<std::string> initialVec{"thing", "stuff"};
    lua["newvec"] = initialVec;
    std::vector<std::string> reconstructedVec = lua["newvec"];
    ASSERT_EQ(initialVec, reconstructedVec);
}

TEST(LuaBind, Tuple) {
    luabind::Lua lua;
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

TEST(LuaBind, TupleFromLuaFunction) {
    luabind::Lua lua;
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