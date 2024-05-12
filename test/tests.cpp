//
// Created by Matan Silver on 5/29/23.
//
#include "luabind/luabind.hpp"
#include "gtest/gtest.h"

using namespace luabind;
using namespace luabind::detail;

// Static asserts act as unittests for compile-time functionality
static_assert(!traits::always_false_v<int>);

struct CallableStruct {
    char operator()(int, bool) { return 'a'; }

    char nonConstMethod(int, bool) { return 'a'; }

    char constMethod(int, bool) const { return 'a'; }

    static char staticMethod(int, bool) { return 'a'; }
};

static_assert(std::is_same_v<traits::function_traits<decltype([](int, bool) {})>::return_type, void>);
static_assert(std::is_same_v<traits::function_traits<decltype([](int,
                                                                 bool) { return 1; })>::return_type, int>);
static_assert(std::is_same_v<traits::function_traits<decltype([](int,
                                                                 bool) { return 1; })>::argument_types, std::tuple<int, bool>>);

static_assert(std::is_same_v<traits::function_traits<void (*)(int, bool)>::return_type, void>);
static_assert(std::is_same_v<traits::function_traits<int (*)(int, bool)>::return_type, int>);
static_assert(std::is_same_v<traits::function_traits<int (*)(int,
                                                             bool)>::argument_types, std::tuple<int, bool>>);

static_assert(std::is_same_v<traits::function_traits<CallableStruct>::return_type, char>);
static_assert(
        std::is_same_v<traits::function_traits<decltype(&CallableStruct::constMethod)>::return_type, char>);
static_assert(
        std::is_same_v<traits::function_traits<decltype(&CallableStruct::nonConstMethod)>::return_type, char>);
static_assert(
        std::is_same_v<traits::function_traits<decltype(&CallableStruct::staticMethod)>::return_type, char>);
static_assert(
        std::is_same_v<traits::function_traits<decltype(&CallableStruct::constMethod)>::argument_types, std::tuple<int, bool>>);
static_assert(
        std::is_same_v<traits::function_traits<decltype(&CallableStruct::nonConstMethod)>::argument_types, std::tuple<int, bool>>);
static_assert(
        std::is_same_v<traits::function_traits<decltype(&CallableStruct::staticMethod)>::argument_types, std::tuple<int, bool>>);

static_assert(traits::is_vector_v<std::vector<int>>);
static_assert(traits::is_vector_v<std::vector<std::string>>);
static_assert(traits::is_vector_v<std::vector<std::vector<int>>>);
static_assert(!traits::is_vector_v<int>);
static_assert(!traits::is_vector_v<std::string>);
static_assert(!traits::is_vector_v<void>);

static_assert(traits::is_tuple_v<std::tuple<int>>);
static_assert(traits::is_tuple_v<std::tuple<int, bool>>);
static_assert(traits::is_tuple_v<std::tuple<std::string>>);
static_assert(traits::is_tuple_v<std::tuple<std::vector<int>>>);
static_assert(!traits::is_tuple_v<std::vector<std::tuple<>>>);
static_assert(!traits::is_tuple_v<std::string>);
static_assert(!traits::is_tuple_v<void>);

TEST(LuaBind, Call) {
    luabind::Lua lua;
    lua << R"(
            myprint = function(a, b)
                print(a, b)
            end
        )";
    lua["myprint"]("thing", "stuff");
}

TEST(LuaBind, CallRNumberArg) {
    luabind::Lua lua;
    lua << R"(
            myadd = function(a, b)
                return a+b
            end
        )";
    int x = lua["myadd"](1, 2);
    ASSERT_EQ(x, 3);
}

TEST(LuaBind, CallRBooleanArg) {
    luabind::Lua lua;
    lua << R"(
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
    lua << R"(
            identity = function(a)
                return a
            end
        )";
    std::string x = lua["identity"](std::string("thing"));
    ASSERT_EQ(x, std::string{"thing"});
}

TEST(LuaBind, CallRCharPtrArg) {
    luabind::Lua lua;
    lua << R"(
            identity = function(a)
                return a
            end
        )";
    std::string x = lua["identity"]("thing");
    ASSERT_EQ(x, std::string{"thing"});
}

TEST(LuaBind, CallRCharArg) {
    luabind::Lua lua;
    lua << R"(
            identity = function(a)
                return a
            end
        )";
    char x = lua["identity"]('b');
    ASSERT_EQ(x, 'b');
}

TEST(LuaBind, Expose) {
    luabind::Lua lua;
    lua["timesTwo"] = [](int x) -> int {
        return x * 2;
    };
    lua << R"(
            callIntoCFunc = function(a)
                return timesTwo(a)
            end
        )";
    int x = lua["callIntoCFunc"](4);
    ASSERT_EQ(x, 8);
}

TEST(LuaBind, ExposeCaptures) {
    luabind::Lua lua;
    int x = 0;
    lua["incrementX"] = [&x]() {
        ++x;
    };
    lua["incrementX"]();
    ASSERT_EQ(x, 1);
}

TEST(LuaBind, ExposeCapturesShared) {
    luabind::Lua lua1;
    luabind::Lua lua2;
    int x = 0;
    auto lam = [&x]() {
        ++x;
    };
    lua1["incrementX"] = lam;
    lua2["incrementX"] = lam;
    lua1["incrementX"]();
    ASSERT_EQ(x, 1);
    lua2["incrementX"]();
    ASSERT_EQ(x, 2);
}

int add(int a, int b) {
    return a + b;
}

TEST(LuaBind, ExposeCStyleFunction) {
    luabind::Lua lua;
    lua["myadd"] = &add;
    ASSERT_EQ((int) lua["myadd"](1, 2), 3);
}

TEST(LuaBind, MultipleExposeSameSignature) {
    luabind::Lua lua;
    lua["timesTwo"] = [](int x) -> int {
        return x * 2;
    };
    lua["timesThree"] = [](int x) -> int {
        return x * 3;
    };
    lua << R"(
            callIntoCFunc = function(a)
                return timesTwo(a) + timesThree(a)
            end
        )";
    int x = lua["callIntoCFunc"](4);
    ASSERT_EQ(x, 20);

}

TEST(LuaBind, IncorrectArgumentType) {
    luabind::Lua lua;
    lua["timesTwo"] = [](int x) -> int {
        return x * 2;
    };
    auto willThrow = [&]() { [[maybe_unused]] int x = lua["timesTwo"](std::string("thing")); };
    ASSERT_THROW(willThrow(), IncorrectType);
}

TEST(LuaBind, IncorrectReturnType) {
    luabind::Lua lua;
    lua << R"(
            identity = function(a)
                return a
            end
        )";
    auto willThrow = [&]() { std::string x = lua["identity"](1); };
    ASSERT_THROW(willThrow(), IncorrectType);
}

TEST(LuaBind, IncorrectTypeBool) {
    luabind::Lua lua;
    lua << R"(
        isNotBool = "thing"
    )";
    auto willThrow = [&]() { bool x = lua["isNotBool"]; };
    ASSERT_THROW(willThrow(), IncorrectType);
}

TEST(LuaBind, IncorrectTypeVector) {
    luabind::Lua lua;
    lua << R"(
        isNotVector = "thing"
    )";
    auto willThrow = [&]() { std::vector<int> x = lua["isNotVector"]; };
    ASSERT_THROW(willThrow(), IncorrectType);
}

TEST(LuaBind, GetGlobalValue) {
    luabind::Lua lua;
    lua << R"(
            foo=4
        )";
    int foo = lua["foo"];
    ASSERT_EQ(foo, 4);
}

TEST(LuaBind, LambdaWithCapturedState) {
    luabind::Lua lua;
    int accumulator{0};
    lua["incrementAccumulator"] = [&accumulator]() { accumulator++; };

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
     ["sumAB"] = [](int a, int b) { return a + b; })
            << R"(
              sumXY = function()
                  return sumAB(x,y)
              end
              )";
    ASSERT_EQ((int) lua["sumXY"](), 3);
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