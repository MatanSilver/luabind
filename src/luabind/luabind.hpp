#ifndef LUABIND_LUABIND_HPP
#define LUABIND_LUABIND_HPP

#include "lua.hpp"

#include <concepts>
#include <optional>
#include <tuple>
#include <type_traits>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <string>


namespace luabind::detail::traits {
    template<typename T>
    inline constexpr bool always_false_v = false;

    template<typename Callable>
    struct function_traits;

    template<typename ReturnType, typename... Args>
    struct function_traits<ReturnType (*)(Args...)> {
        using return_type = ReturnType;
        using argument_types = std::tuple<Args...>;
    };

    template<typename ReturnType, typename ClassType, typename... Args>
    struct function_traits<ReturnType (ClassType::*)(Args...)> {
        using return_type = ReturnType;
        using argument_types = std::tuple<Args...>;
    };

    template<typename ReturnType, typename ClassType, typename... Args>
    struct function_traits<ReturnType (ClassType::*)(Args...) const> {
        using return_type = ReturnType;
        using argument_types = std::tuple<Args...>;
    };

    template<typename Callable>
    struct function_traits {
    private:
        using call_type = decltype(&Callable::operator());
    public:
        using return_type = typename function_traits<call_type>::return_type;
        using argument_types = typename function_traits<call_type>::argument_types;
    };

    template<typename>
    struct is_vector : std::false_type {
    };

    template<typename T>
    struct is_vector<std::vector<T>> : std::true_type {
    };

    template<typename T>
    constexpr bool is_vector_v = is_vector<T>::value;


    template<typename>
    struct is_tuple : std::false_type {
    };

    template<typename ...Args>
    struct is_tuple<std::tuple<Args...>> : std::true_type {
    };

    template<typename T>
    constexpr bool is_tuple_v = is_tuple<T>::value;
}

namespace luabind::detail {
    typedef int (*FuncPtr)(lua_State *);

    template<typename Callable, typename UniqueType = decltype([]() {})>
    FuncPtr adapt(const Callable &aFunc);

    template<typename T>
    void toLua(lua_State *aState, T &&aVal);

    template<typename T>
    T fromLua(lua_State *aState);

    template<typename T>
    void setTableElement(lua_State *aState, const T &aVal, int i) {
        toLua(aState, aVal);
        lua_seti(aState, -2, i);
    }

    template<typename T>
    T getTableElement(lua_State *aState, int i) {
        lua_geti(aState, -1, i);
        return fromLua<T>(aState);
    }

    template<typename ...Args, std::size_t... I>
    void toLuaTupleHelper(lua_State *aState, const std::tuple<Args...> &aVal, std::index_sequence<I...>) {
        (setTableElement(aState, std::get<I>(aVal), I + 1), ...);
    }

    template<typename T>
    void toLuaTuple(lua_State *aState, const T &aVal) {
        lua_createtable(aState, std::tuple_size_v<std::decay_t<T>>, 0);
        toLuaTupleHelper(aState, aVal, std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>());
    }

    template<typename T, std::size_t... I>
    T fromLuaTupleHelper(lua_State *aState, std::index_sequence<I...>) {
        return {getTableElement<std::tuple_element_t<I, T>>(aState, I + 1) ...};
    }

    template<typename T>
    T fromLuaTuple(lua_State *aState) {
        constexpr std::size_t tuple_size = std::tuple_size_v<T>;
        return fromLuaTupleHelper<T>(aState, std::make_index_sequence<tuple_size>());
    }

    template<typename T>
    void toLua(lua_State *aState, T &&aVal) {
        if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            lua_pushboolean(aState, std::forward<T>(aVal));
        } else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
            lua_pushnumber(aState, std::forward<T>(aVal));
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            lua_pushlstring(aState, aVal.c_str(), aVal.length());
        } else if constexpr (std::is_same_v<std::decay_t<T>, char>) {
            lua_pushlstring(aState, &aVal, 1);
        } else if constexpr (std::is_same_v<std::decay_t<T>, char *> || std::is_same_v<std::decay_t<T>, const char *>) {
            lua_pushstring(aState, std::forward<T>(aVal));
        } else if constexpr (traits::is_vector_v<std::decay_t<T>>) {
            lua_createtable(aState, aVal.size(), 0);
            for (int i = 0; i < aVal.size(); ++i) {
                setTableElement(aState, aVal[i], i + 1);
            }
        } else if constexpr (traits::is_tuple_v<std::decay_t<T>>) {
            toLuaTuple(aState, std::forward<T>(aVal));
        } else {
            // TODO: somehow check if T is a callable, and static assert for other unsupported types
            lua_pushcfunction(aState, detail::adapt(std::forward<T>(aVal)));
        }
    }

    struct IncorrectType : std::runtime_error {
        IncorrectType() : std::runtime_error("Incorrect type") {}
    };

    template<typename T>
    T fromLua(lua_State *aState) {
        if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            if (!lua_isboolean(aState, -1)) {
                throw IncorrectType();
            }
            T ret = lua_toboolean(aState, -1);
            lua_pop(aState, 1);
            return ret;
        } else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
            if (!lua_isnumber(aState, -1)) {
                throw IncorrectType();
            }
            T ret = lua_tonumber(aState, -1);
            lua_pop(aState, 1);
            return ret;
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            if (!lua_isstring(aState, -1) || lua_isnumber(aState, -1)) {
                // lua_isstring returns true for numbers, oddly
                throw IncorrectType();
            }
            T ret = lua_tostring(aState, -1);
            lua_pop(aState, 1);
            return ret;
            // We don't support const char* for memory safety reasons
        } else if constexpr (std::is_same_v<std::decay_t<T>, char>) {
            if (!lua_isstring(aState, -1) || lua_isnumber(aState, -1)) {
                // lua_isstring returns true for numbers, oddly
                throw IncorrectType();
            }
            size_t len;
            const char *buf = lua_tolstring(aState, -1, &len);
            if (len != 1) {
                throw IncorrectType();
            }
            lua_pop(aState, 1);
            return buf[0];
        } else if constexpr (traits::is_vector_v<std::decay_t<T>>) {
            if (!lua_istable(aState, -1)) {
                throw IncorrectType();
            }
            T retVec;
            for (int i = 0; i < lua_rawlen(aState, -1); ++i) {
                retVec.emplace_back(getTableElement<typename T::value_type>(aState, i + 1));
            }
            return retVec;
        } else if constexpr (traits::is_tuple_v<std::decay_t<T>>) {
            return fromLuaTuple<std::decay_t<T>>(aState);
        } else {
            static_assert(detail::traits::always_false_v<T>, "Unsupported type");
        }
    }

    template<typename Callable, typename> std::optional<Callable> gCallable;

    template<typename ...Args>
    std::tuple<Args...> getArgsAsTuple(lua_State *aState, std::tuple<Args...>) {
        return {detail::fromLua<Args>(aState) ...};
    };

    template<typename Callable, typename UniqueType>
    int adapted(lua_State *aState) {
        using retType = typename detail::traits::function_traits<Callable>::return_type;
        using argTypes = typename detail::traits::function_traits<Callable>::argument_types;

        if constexpr (std::is_same_v<retType, void>) {
            std::apply(*gCallable<Callable, UniqueType>, getArgsAsTuple(aState, argTypes()));
            return 0;
        } else {
            toLua(aState, std::apply(*gCallable<Callable, UniqueType>, getArgsAsTuple(aState, argTypes())));
            return 1;
        }
    }

    template<typename Callable, typename UniqueType>
    FuncPtr adapt(const Callable &aFunc) {
        gCallable<Callable, UniqueType>.emplace(aFunc);
        return &adapted<Callable, UniqueType>;
    }

    class RetHelper {
    public:
        RetHelper(lua_State *aState) : fState(aState), fWasCasted(false) {}

        ~RetHelper() {
            // If we never popped off the stack, assumptions about
            // the stack size may be incorrect
            assert(fWasCasted);
        }

        template<typename T>
        operator T() {
            assert(!fWasCasted); // We can only pop off the stack once
            fWasCasted = true;
            return detail::fromLua<T>(fState);
        }

    private:
        lua_State *fState;
        bool fWasCasted;
    };
}

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

        Lua &operator<<(const std::string &aCodeToRun) {
            loadScript(aCodeToRun);
            return *this;
        }

        struct GetGlobalHelper {
            GetGlobalHelper(const std::string &aGlobalName, Lua &aLua)
                    : fGlobalName(aGlobalName), fLua(aLua) {}

            template<typename ...Args>
            auto operator()(const Args &... args) {
                return fLua.call(fGlobalName, args...);
            }

            template<typename T>
            operator T() {
                lua_getglobal(fLua.fState, fGlobalName.c_str());
                return detail::fromLua<T>(fLua.fState);
            }

            template<typename T>
            Lua &operator=(T &&aVal) {
                detail::toLua(fLua.fState, std::forward<T>(aVal));
                lua_setglobal(fLua.fState, fGlobalName.c_str());
                return fLua;
            }

        private:
            const std::string fGlobalName;
            Lua &fLua;
        };

        /*
         * Gets the global of the name aGlobalName
         */
        auto operator[](const std::string &aGlobalName) {
            return GetGlobalHelper{aGlobalName, *this};
        }

    private:
        template<typename ...Args>
        void pushFunctionAndArgs(const std::string &aFunctionName, const Args &... args) {
            // push function on stack
            lua_getglobal(fState, aFunctionName.c_str());
            assert(lua_isfunction(fState, -1) || lua_iscfunction(fState, -1));
            // push args on stack, in order left to right
            (detail::toLua(fState, args), ...);
        }

        template<typename ...Args>
        void callWithoutReturnValue(const std::string &aFunctionName, const Args &... args) {
            pushFunctionAndArgs(aFunctionName, args...);
            lua_call(fState, sizeof...(args), 0);
        }

        template<typename ...Args>
        auto callWithReturnValue(const std::string &aFunctionName, const Args &... args) {
            pushFunctionAndArgs(aFunctionName, args...);
            lua_call(fState, sizeof...(args), 1);
            return detail::RetHelper(fState);
        }

        void loadScript(const std::string &aScript) {
            [[maybe_unused]] auto res = luaL_dostring(fState, aScript.c_str());
            assert(res == LUA_OK);
        }

        // lazy function callWithoutReturnValue evaluation helper
        // also handles functions with/without return values based on
        // the casting history
        template<typename ...Args>
        struct CallSpecialHelper {
            CallSpecialHelper(Lua &aLua, const std::string &aFunctionName, const std::tuple<Args...> &aArgs)
                    : fLua(aLua), fFunctionName(aFunctionName), fArgs{aArgs}, fWasCasted(false) {}

            ~CallSpecialHelper() {
                // If we never called the operator T(), we should run the function with no return val
                if (!fWasCasted) {
                    auto lam = [&](const auto &... args) { fLua.callWithoutReturnValue(args...); };
                    std::apply(lam, std::tuple_cat(std::tuple{fFunctionName}, fArgs));
                }
            }

            template<typename T>
            operator T() {
                fWasCasted = true;
                auto lam = [&](const auto &... args) { return fLua.callWithReturnValue(args...); };
                return std::apply(lam, std::tuple_cat(std::tuple{fFunctionName}, fArgs));
            }

        private:
            Lua &fLua;
            const std::string fFunctionName;
            const std::tuple<Args...> fArgs;
            bool fWasCasted;
        };

        template<typename ...Args>
        auto call(const std::string &aFunctionName, const Args &... args) {
            return CallSpecialHelper{*this, aFunctionName, std::tuple{args...}};
        }

    private:
        lua_State *fState;
    };
}

#endif //LUABIND_LUABIND_HPP
