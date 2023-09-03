#ifndef LUABIND_LUABIND_HPP
#define LUABIND_LUABIND_HPP

#include "detail.hpp"
#include "lua.hpp"
#include <string>

namespace luabind {
    class Lua {
    public:
        Lua() {
            fState = luaL_newstate();
            luaL_openlibs(fState);
        }
        ~Lua() {
            lua_close(fState);
        }

        Lua& operator<<(const std::string& aCodeToRun) {
            loadScript(aCodeToRun);
            return *this;
        }

        struct GetGlobalHelper {
            GetGlobalHelper(const std::string& aFunctionToCall, Lua& aLua)
                    : fGlobalName(aFunctionToCall)
                    , fLua(aLua) {}
            template <typename ...Args>
            auto operator() (const Args&... args) {
                return fLua.call(fGlobalName, args...);
            }

            template <typename T>
            operator T() {
                lua_getglobal(fLua.fState, fGlobalName.c_str());
                return detail::fromLua<T>(fLua.fState);
            }

            template <typename T>
            Lua& operator=(T&& aVal) {
                detail::toLua(fLua.fState, std::forward<T>(aVal));
                lua_setglobal(fLua.fState, fGlobalName.c_str());
                return fLua;
            }

        private:
            const std::string fGlobalName;
            Lua& fLua;
        };

        /*
         * Gets the global of the name aFunctionToCall
         * Could be generalized to get any global, not just functions
         */
        auto operator[](const std::string& aFunctionToCall) {
            return GetGlobalHelper{aFunctionToCall, *this};
        }

    private:
        template <typename ...Args>
        void pushFunctionAndArgs(const std::string& aFunctionName, const Args&... args) {
            // push function on stack
            lua_getglobal(fState, aFunctionName.c_str());
            assert(lua_isfunction(fState, -1) || lua_iscfunction(fState, -1));
            // push args on stack, in order left to right
            (detail::toLua(fState, args), ...);
        }

        template <typename ...Args>
        void callx(const std::string& aFunctionName, const Args&... args) {
            pushFunctionAndArgs(aFunctionName, args...);
            lua_call(fState, sizeof...(args), 0);
        }

        template <typename ...Args>
        auto callxx(const std::string& aFunctionName, const Args&... args) {
            pushFunctionAndArgs(aFunctionName, args...);
            lua_call(fState, sizeof...(args), 1);
            return detail::RetHelper(fState);
        }

        void loadScript(const std::string& aScript) {
            assert(luaL_dostring(fState, aScript.c_str()) == LUA_OK);
        }

        // lazy function callx evaluation helper
        // also handles functions with/without return values based on
        // the casting history
        template <typename ...Args>
        struct CallSpecialHelper {
            CallSpecialHelper(Lua& aLua, const std::string& aFunctionName, const std::tuple<Args...>& aArgs)
                    : fLua(aLua)
                    , fFunctionName(aFunctionName)
                    , fArgs{aArgs}
                    , fWasCasted(false) {}
            ~CallSpecialHelper() {
                // If we never called the operator T(), we should run the function with no return val
                if (!fWasCasted) {
                    auto lam = [&](const auto&... args){ fLua.callx(args...);};
                    std::apply(lam, std::tuple_cat(std::tuple{fFunctionName}, fArgs));
                }
            }

            template <typename T>
            operator T() {
                fWasCasted = true;
                auto lam = [&](const auto&... args){return fLua.callxx(args...);};
                return std::apply(lam, std::tuple_cat(std::tuple{fFunctionName}, fArgs));
            }
        private:
            Lua& fLua;
            const std::string fFunctionName;
            const std::tuple<Args...> fArgs;
            bool fWasCasted;
        };

        template <typename ...Args>
        auto call(const std::string& aFunctionName, const Args&... args) {
            return CallSpecialHelper{*this, aFunctionName, std::tuple{args...}};
        }
    private:
        lua_State *fState;
    };
}

#endif //LUABIND_LUABIND_HPP
