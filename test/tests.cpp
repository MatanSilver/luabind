//
// Created by Matan Silver on 5/29/23.
//
#include "luabind/luabind.hpp"
#include "gtest/gtest.h"

#include <array>

static const char *gIdentityFunction = R"(
    identity = function(a)
        return a
    end
)";

TEST(LuaBind, Call) {
  luabind::Lua lua;
  lua << R"(
            printFunc = function(a, b)
                print(a, b)
            end
        )";
  lua["printFunc"]("thing", "stuff");
}

TEST(LuaBind, CallRNumberArg) {
  luabind::Lua lua;
  lua << R"(
            addFunc = function(a, b)
                return a+b
            end
        )";
  int x = lua["addFunc"](1, 2);
  ASSERT_EQ(x, 3);
}

TEST(LuaBind, CallRBooleanArg) {
  luabind::Lua lua;
  lua << R"(
            andFunc = function(a, b)
                return a and b
            end
        )";
  bool x = lua["andFunc"](true, false);
  ASSERT_EQ(x, false);

  bool y = lua["andFunc"](true, true);
  ASSERT_EQ(y, true);
}

TEST(LuaBind, CallRStringArg) {
  luabind::Lua lua;
  lua << gIdentityFunction;
  std::string x = lua["identity"](std::string("thing"));
  ASSERT_EQ(x, std::string{"thing"});
}

TEST(LuaBind, CallRCharPtrArg) {
  luabind::Lua lua;
  lua << gIdentityFunction;
  std::string x = lua["identity"]("thing");
  ASSERT_EQ(x, std::string{"thing"});
}

TEST(LuaBind, CallRCharArg) {
  luabind::Lua lua;
  lua << gIdentityFunction;
  char x = lua["identity"]('b');
  ASSERT_EQ(x, 'b');
}

TEST(LuaBind, Expose) {
  luabind::Lua lua;
  lua["timesTwo"] = [](int x) -> int {
    return x*2;
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
  lua["addFunc"] = &add;
  ASSERT_EQ((int)lua["addFunc"](1, 2), 3);
}

TEST(LuaBind, MultipleExposeSameSignature) {
  luabind::Lua lua;
  lua["timesTwo"] = [](int x) -> int {
    return x*2;
  };
  lua["timesThree"] = [](int x) -> int {
    return x*3;
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
    return x*2;
  };
  auto willThrow = [&]() { [[maybe_unused]] int x = lua["timesTwo"](std::string("thing")); };
  ASSERT_THROW(willThrow(), luabind::RuntimeError);
}

TEST(LuaBind, IncorrectReturnType) {
  luabind::Lua lua;
  lua << gIdentityFunction;
  auto willThrow = [&]() { std::string x = lua["identity"](1); };
  ASSERT_THROW(willThrow(), luabind::IncorrectType);
}

TEST(LuaBind, IncorrectTypeBool) {
  luabind::Lua lua;
  lua << R"(
        isNotBool = "thing"
    )";
  auto willThrow = [&]() { bool x = lua["isNotBool"]; };
  ASSERT_THROW(willThrow(), luabind::IncorrectType);
}

TEST(LuaBind, IncorrectTypeVector) {
  luabind::Lua lua;
  lua << R"(
        isNotVector = "thing"
    )";
  auto willThrow = [&]() { std::vector<int> x = lua["isNotVector"]; };
  ASSERT_THROW(willThrow(), luabind::IncorrectType);
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
  ASSERT_EQ((int)lua["sumXY"](), 3);
}

template <typename T>
T roundTrip(T const &aCppVal, luabind::Lua &aLua) {
  aLua["luaVal"] = aCppVal;
  return aLua["luaVal"];
}

template <typename T>
T roundTrip(T const &aCppVal) {
  luabind::Lua lua;
  return roundTrip(aCppVal, lua);
}

TEST(LuaBind, IntVector) {
  std::vector initialVec{1, 3, 4, 5};
  ASSERT_EQ(initialVec, roundTrip(initialVec));
}

TEST(LuaBind, StringVector) {
  std::vector<std::string> initialVec{"thing", "stuff"};
  ASSERT_EQ(initialVec, roundTrip(initialVec));
}

TEST(LuaBind, Tuple) {
  // We can even have tuples containing tuples!
  using T = std::tuple<std::string,
                       int,
                       bool,
                       std::tuple<int, bool>>;
  T initialTuple{"thing", 1, true, {4, false}};
  ASSERT_EQ(initialTuple, roundTrip(initialTuple));
}

TEST(LuaBind, TupleVector) {
  std::vector<std::tuple<int, std::string>> initialVector{
      {0, "zero"},
      {1, "one"}
  };
  ASSERT_EQ(initialVector, roundTrip(initialVector));
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

TEST(LuaBind, CppFunctionCallsLua) {
  luabind::Lua lua;
  lua << R"(
        accumulator = 0
        accumulateLua = function()
            accumulator = accumulator + 1
        end
    )";
  lua["accumulateCpp"] = [&lua]() {
    lua << "accumulateLua()";
  };
  lua << "accumulateCpp()";
  lua << "accumulateCpp()";
  ASSERT_EQ((int)lua["accumulator"], 2);
}

TEST(LuaBind, CanContinueAfterError) {
  luabind::Lua lua;
  lua << R"(
            doError = function()
                error("foo")
            end
        )";
  auto willThrow = [&lua]() {
    lua << "doError()";
  };
  ASSERT_THROW(willThrow(), luabind::RuntimeError);

  lua << "globalVal = 3";
  ASSERT_EQ((int)lua["globalVal"], 3);
}

TEST(LuaBind, SyntaxError) {
  luabind::Lua lua;
  auto willThrow = [&lua]() { lua << "foo("; };
  ASSERT_THROW(willThrow(), luabind::SyntaxError);
}

TEST(LuaBind, MetaStruct) {
  using namespace luabind::meta::literals;
  using namespace luabind::meta;

  meta_struct<
      meta_field<"biz"_f, int>,
      meta_field<"buz"_f, bool>> bar{{1}, {false}};

  auto x = bar.f<"biz"_f>();
  auto y = bar.f<"buz"_f>();

  ASSERT_EQ(x, 1);
  ASSERT_EQ(y, false);

  bar.f<"buz"_f>() = true;
  ASSERT_EQ(bar.f<"buz"_f>(), true);

  bar.f<"biz"_f>() = 10;
  ASSERT_EQ(bar.f<"biz"_f>(), 10);
}

TEST(LuaBind, MetaStructSerDe) {
  using namespace luabind::meta::literals;
  using namespace luabind::meta;

  meta_struct<
      meta_field<"biz"_f, int>,
      meta_field<"buz"_f, bool>> bar{{1}, {false}};

  ASSERT_TRUE(bar==roundTrip(bar));
}

TEST(LuaBind, MetaStructVector) {
  using namespace luabind::meta::literals;
  using namespace luabind::meta;

  using MetaStructType = meta_struct<meta_field<"field1"_f, int>, meta_field<"field2"_f, std::string>>;
  using VectorType = std::vector<MetaStructType>;

  VectorType initialValue{{{0}, {"zero"}}, {{1}, {"one"}}};
  ASSERT_TRUE(initialValue==roundTrip(initialValue));
}

TEST(LuaBind, MetaStructIsReadable) {
  using namespace luabind::meta::literals;
  using namespace luabind::meta;

  meta_struct<
      meta_field<"biz"_f, int>,
      meta_field<"buz"_f, bool>> bar{{1}, {false}};

  luabind::Lua lua;

  lua << R"(
        transform = function(inTable)
            return {foo=inTable.biz, far=inTable.buz}
        end
    )";
  lua["globalStruct"] = bar;
  meta_struct<
      meta_field<"foo"_f, int>,
      meta_field<"far"_f, bool>> expected{{1}, {false}};
  ASSERT_TRUE(expected==lua["transform"](bar));
}

TEST(LuaBind, StackManagement) {
  using namespace luabind::meta::literals;
  using namespace luabind::meta;

  auto l = luaL_newstate();
  luabind::Lua lua(l);
  ASSERT_EQ(lua_gettop(l), 0);
  lua["global"] = 1;
  ASSERT_EQ(lua_gettop(l), 0);
  int i = lua["global"];
  ASSERT_EQ(i, 1);
  ASSERT_EQ(lua_gettop(l), 0);
  lua << R"(
    someFunc = function(arg)
      return arg == "blah"
    end
  )";
  ASSERT_EQ(lua_gettop(l), 0);
  bool isSame = lua["someFunc"]("blah");
  ASSERT_TRUE(isSame);
  ASSERT_EQ(lua_gettop(l), 0);
  using MostComplexType = meta_struct<meta_field<"structField"_f, std::tuple<int, std::vector<std::string>>>>;
  MostComplexType original{{{1, {"elem1", "elem2"}}}};
  ASSERT_TRUE(original==roundTrip(original, lua));
  ASSERT_EQ(lua_gettop(l), 0);
}

int main(int aArgc, char **aArgv) {
  ::testing::InitGoogleTest(&aArgc, aArgv);
  return RUN_ALL_TESTS();
}