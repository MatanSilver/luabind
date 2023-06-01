#ifndef LUAWRAPPER_LUAWRAPPER_HPP
#define LUAWRAPPER_LUAWRAPPER_HPP

#include "detail.hpp"

#include "lua.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace luawrapper {
    class Lua {
    public:
        Lua() {
            fState = luaL_newstate();
            luaL_openlibs(fState);
        }
        ~Lua() {
            lua_close(fState);
        }

        void loadScriptFile(const std::string& aFileName) {
            assert(luaL_dofile(fState, aFileName.c_str()) == LUA_OK);
        }

        void loadScript(const std::string& aScript) {
            assert(luaL_dostring(fState, aScript.c_str()) == LUA_OK);
        }

        template <typename ...Args>
        void call(const std::string& aFunctionName, const Args&... args) {
            pushFunctionAndArgs(aFunctionName, args...);
            lua_call(fState, sizeof...(args), 0);
        }

        template <typename ...Args>
        auto callr(const std::string& aFunctionName, const Args&... args) {
            pushFunctionAndArgs(aFunctionName, args...);
            lua_call(fState, sizeof...(args), 1);
            return detail::RetHelper(fState);
        }

        template <typename Callable>
        void expose(const std::string& aFunctionName, Callable aFunction) {
            lua_register(fState, aFunctionName.c_str(), aFunction);
        }

    private:
        template <typename ...Args>
        void pushFunctionAndArgs(const std::string& aFunctionName, const Args&... args) {
            // push function on stack
            lua_getglobal(fState, aFunctionName.c_str());
            assert(lua_isfunction(fState, -1));
            // push args on stack, in order left to right
            (detail::toLua(fState, args), ...);
        }
    private:
        lua_State *fState;
    };

}

#endif //LUAWRAPPER_LUAWRAPPER_HPP
