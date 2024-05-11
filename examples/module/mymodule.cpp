//
// Created by Matan Silver on 5/11/24.
//

#include "luabind/luabind.hpp"
#include "lauxlib.h"

static const struct luaL_Reg functions [] = {
        { "say_hello", luabind::detail::adapt([](){ return "hello world!"; })},
        {NULL, NULL}
};

extern "C" {

int luaopen_mymodule(lua_State *L) {
    luaL_newlib(L, functions);
    lua_setglobal(L, "mymodule");
    return 1;
}

}
