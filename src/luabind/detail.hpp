//
// Created by Matan Silver on 5/30/23.
//

#ifndef LUABIND_DETAIL_HPP
#define LUABIND_DETAIL_HPP

#include "lua.hpp"
#include "luabind/traits.hpp"

#include <concepts>
#include <string>

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
#endif //LUABIND_DETAIL_HPP
