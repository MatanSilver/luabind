//
// Created by Matan Silver on 5/30/23.
//

#ifndef LUAWRAPPER_DETAIL_HPP
#define LUAWRAPPER_DETAIL_HPP

#include "lua.hpp"
#include "traits.hpp"

#include <concepts>

namespace luawrapper::detail {
    typedef int (*FuncPtr)(lua_State *);

    template<typename Callable, typename UniqueType = decltype([](){})>
    FuncPtr adapt(const Callable &aFunc);

    template<typename T>
    void toLua(lua_State *aState, T&& aVal) {
        if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            lua_pushboolean(aState, std::forward<T>(aVal));
        } else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
            lua_pushnumber(aState, std::forward<T>(aVal));
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            lua_pushlstring(aState, aVal.c_str(), aVal.length());
        } else if constexpr (std::is_same_v<std::decay_t<T>, char *> || std::is_same_v<std::decay_t<T>, const char *>) {
            lua_pushstring(aState, std::forward<T>(aVal));
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
        T ret;
        if constexpr (std::is_same_v<T, bool>) {
            if (!lua_isboolean(aState, -1)) {
                throw IncorrectType();
            }
            ret = lua_toboolean(aState, -1);
            lua_pop(aState, 1);
            return ret;
        } else if constexpr (std::is_arithmetic_v<T>) {
            if (!lua_isnumber(aState, -1)) {
                throw IncorrectType();
            }
            ret = lua_tonumber(aState, -1);
            lua_pop(aState, 1);
            return ret;
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (!lua_isstring(aState, -1) || lua_isnumber(aState, -1)) {
                // lua_isstring returns true for numbers, oddly
                throw IncorrectType();
            }
            ret = lua_tostring(aState, -1);
            lua_pop(aState, 1);
            return ret;
            // We don't support const char* for memory safety reasons
        } else {
            static_assert(detail::traits::always_false_v<T>, "Unsupported type");
        }
    }

    template<typename Callable, typename> std::optional<Callable> gCallable;

    template <typename>
    struct get_args_as_tuple{};

    template <typename ...Args>
    struct get_args_as_tuple<std::tuple<Args...>> {
        static std::tuple<Args...> apply(lua_State *aState) {
            return {detail::fromLua<Args>(aState) ...};
        }
    };


    template<typename Callable, typename UniqueType>
    int adapted(lua_State *aState) {
        using retType = typename detail::traits::function_traits<Callable>::return_type;
        using argTypes = typename detail::traits::function_traits<Callable>::argument_types;

        auto args = get_args_as_tuple<argTypes>::apply(aState);
        if constexpr (std::is_same_v<retType, void>) {
            std::apply(*gCallable<Callable, UniqueType>, args);
            return 0;
        } else {
            retType ret = std::apply(*gCallable<Callable, UniqueType>, args);
            toLua(aState, ret);
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
#endif //LUAWRAPPER_DETAIL_HPP
