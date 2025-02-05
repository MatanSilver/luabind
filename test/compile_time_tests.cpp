//
// Created by Matan on 6/8/2024.
//

#include "luabind/luabind.hpp"

using namespace luabind::detail;
using namespace luabind::meta;
using namespace luabind::meta::literals;

// Static asserts act as unittests for compile-time functionality
static_assert(!traits::always_false_v<int>);

struct CallableStruct {
  char operator()(int, bool) { return 'a'; }

  char nonConstMethod(int, bool) { return 'a'; }

  char constMethod(int, bool) const { return 'a'; }

  static char staticMethod(int, bool) { return 'a'; }
};

struct NonCallableStruct {};

static_assert(traits::is_callable_v<decltype([](int, bool) {})>);
static_assert(traits::is_callable_v<decltype([](int, bool) { return 1; })>);
static_assert(traits::is_callable_v<void (*)(int, bool)>);
static_assert(traits::is_callable_v<int (*)(int, bool)>);
static_assert(traits::is_callable_v<CallableStruct>);
static_assert(traits::is_callable_v<decltype(&CallableStruct::constMethod)>);
static_assert(traits::is_callable_v<decltype(&CallableStruct::nonConstMethod)>);
static_assert(traits::is_callable_v<decltype(&CallableStruct::staticMethod)>);
static_assert(!traits::is_callable_v<int>);
static_assert(!traits::is_callable_v<NonCallableStruct>);

static_assert(std::is_same_v<traits::function_traits<decltype([](int, bool) {})>::ReturnType, void>);
static_assert(std::is_same_v<traits::function_traits<decltype([](int,
                                                                 bool) { return 1; })>::ReturnType, int>);
static_assert(std::is_same_v<traits::function_traits<decltype([](int,
                                                                 bool) { return 1; })>::ArgumentTypes,
                             std::tuple<int, bool>>);

static_assert(std::is_same_v<traits::function_traits<void (*)(int, bool)>::ReturnType, void>);
static_assert(std::is_same_v<traits::function_traits<int (*)(int, bool)>::ReturnType, int>);
static_assert(std::is_same_v<traits::function_traits<int (*)(int,
                                                             bool)>::ArgumentTypes, std::tuple<int, bool>>);

static_assert(std::is_same_v<traits::function_traits<CallableStruct>::ReturnType, char>);
static_assert(
    std::is_same_v<traits::function_traits<decltype(&CallableStruct::constMethod)>::ReturnType, char>);
static_assert(
    std::is_same_v<traits::function_traits<decltype(&CallableStruct::nonConstMethod)>::ReturnType, char>);
static_assert(
    std::is_same_v<traits::function_traits<decltype(&CallableStruct::staticMethod)>::ReturnType, char>);
static_assert(
    std::is_same_v<traits::function_traits<decltype(&CallableStruct::constMethod)>::ArgumentTypes,
                   std::tuple<int, bool>>);
static_assert(
    std::is_same_v<traits::function_traits<decltype(&CallableStruct::nonConstMethod)>::ArgumentTypes,
                   std::tuple<int, bool>>);
static_assert(
    std::is_same_v<traits::function_traits<decltype(&CallableStruct::staticMethod)>::ArgumentTypes,
                   std::tuple<int, bool>>);

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

static_assert(traits::is_table_v<table<>>);
static_assert(traits::is_table_v<table<field<"name"_f, int>>>);
static_assert(traits::is_table_v<table<field<"name1"_f, int>, field<"name2"_f, std::string>>>);
static_assert(!traits::is_table_v<int>);
static_assert(!traits::is_table_v<std::string>);