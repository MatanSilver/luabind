//
// Created by Matan Silver on 5/11/24.
//

#include "luabind/luabind.hpp"

#include <string>

static const struct luaL_Reg functions [] = {
        { "say_hello", luabind::detail::adapt([](){ return "hello world!"; })},
        {NULL, NULL}
};

int luaopen_hello(lua_State *L) {
    luaL_register(L, "hello", functions);
    return 1;
}