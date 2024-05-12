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
    /*
     * In order to dispatch to the right type conversions, we need some
     * utilities for determining compile-time information about types.
     * We'll use the "traits" namespace for this purpose.
     */

    template<typename T>
    inline constexpr bool always_false_v = false;

    /*
     * Determining if something is callable is complicated--
     * Something "callable" can be:
     * 1) A c-style function pointer
     * 2) An object with a call operator (e.g. a lambda, or struct with operator())
     * 3) A std::function
     * 4) A method
     * 5) A const method
     * 6) A static method
     * 7) Probably more I'm forgetting
     *
     * We can do our best here by writing specializations for
     * "function_traits" for these options. SFINAE means we can
     * compose these specializations to make a more useful trait
     *
     * We can also use these traits to provide information about
     * argument and return types (or lack thereof).
     */
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

namespace luabind {
    // Writing c-style function pointer types is tedious. Let's typedef this
    typedef int (*FuncPtr)(lua_State *);

    /*
     * luabind::adapt is a clever function that converts a callable argument
     * to a c-style function pointer to a function that adheres to Lua's
     * requirements of taking in a lua_State* and returning an int. The way we do
     * this is by creating a new specialization of a global variable /for every callsite/
     * of luabind::adapt, and storing a reference to the callable in that specialized global
     * variable. The actual implementation of the pointed-to function is a specialization of
     * luabind::detail::adapted associated with the luabind::adapt callsite, which uses
     * compile-time information about the argument and return types of the callable object
     * to serialize and deserialize those values between C++ and Lua domains.
     *
     * The synthetic FuncPtr-style function must maintain a long-lasting reference to the
     * underlying callable passed to luabind::adapt. However, with a strictly defined
     * interface, said reference can't be exposed to the function through an argument.
     * The trick we use to avoid this is to create a templated global (luabind::detail::gCallable)
     * with a "UniqueType" template parameter with a default value equal to the type of
     * an empty lambda. Distinct lambdas are guaranteed to have distinct types. The result
     * is we can implicitly create a new global for every syntactic call to luabind::adapt!
     * Thus, every call to luabind::adapt can store a unique reference to the callable it is
     * provided with, for the luabind::detail::adapted specialization to call when needed.
     */
    template<typename Callable, typename UniqueType = decltype([]() {})>
    FuncPtr adapt(const Callable &aFunc);
}

namespace luabind::detail {
    /*
     * luabind::detail::toLua receives a lua_State and a value in the
     * C++ domain. It then uses the Lua C API to push that value with the
     * right type onto the Lua stack. Lua uses this odd way to pass values
     * back and forth--instead of an opaque pointer like PyObject*, Lua
     * makes you push and pop values from a stack. Odd ergonomics.
     * Because aVal is an input, we can usually deduce its type without requiring
     * the caller to explicitly pass the type as a template argument.
     */
    template<typename T>
    void toLua(lua_State *aState, T &&aVal);

    /*
     * luabind::detail::fromLua receives a lua_State, and based on the template
     * type T will pop a value off the Lua stack and return it in the C++ domain.
     * Callers must provide the type T because it is not associated with an input
     * argument.
     */
    template<typename T>
    T fromLua(lua_State *aState);

    template<typename T>
    void setTableElement(lua_State *aState, const T &aVal, int i) {
        // First push the Lua-domain converted aVal onto the stack
        toLua(aState, aVal);

        // idx -1 on the stack is the converted aVal, -2 is the table
        lua_seti(aState, -2, i);
    }

    template<typename T>
    T getTableElement(lua_State *aState, int i) {
        lua_geti(aState, -1, i);
        return fromLua<T>(aState);
    }

    /*
     * toLuaTupleHelper uses a fold-expression along with a compile-time
     * index_sequence to expand a single syntactic call to setTableElement
     * to N calls, one for each tuple element.
     */
    template<typename ...Args, std::size_t... I>
    void toLuaTupleHelper(lua_State *aState, const std::tuple<Args...> &aVal, std::index_sequence<I...>) {
        (setTableElement(aState, std::get<I>(aVal), I + 1), ...);
    }

    template<typename T>
    void toLuaTuple(lua_State *aState, const T &aVal) {
        lua_createtable(aState, std::tuple_size_v<std::decay_t<T>>, 0);
        toLuaTupleHelper(aState, aVal, std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>());
    }

    /*
     * fromLuaTupleHelper uses a fold-expression along with a compile-time
     * index_sequence to expand a single syntactic call to getTableElement
     * to N calls, one for each tuple element.
     *
     * T is assumed to be a tuple type, and we use a fold expression within
     * an initializer list to construct the return tuple in-place.
    */
    template<typename T, std::size_t... I>
    T fromLuaTupleHelper(lua_State *aState, std::index_sequence<I...>) {
        return {getTableElement<std::tuple_element_t<I, T>>(aState, I + 1) ...};
    }

    template<typename T>
    T fromLuaTuple(lua_State *aState) {
        constexpr std::size_t tuple_size = std::tuple_size_v<T>;
        return fromLuaTupleHelper<T>(aState, std::make_index_sequence<tuple_size>());
    }

    /*
     * Given a value aVal with deduced type T, push the correctly
     * typed value onto the Lua stack. We use "if constexpr" to do
     * compile-time dispatch in a readable way here, in combination
     * with the traits we wrote above. Another approach would be to
     * write multiple specializations of toLua using SFINAE. However,
     * here we have better control over the compile errors.
     */
    template<typename T>
    void toLua(lua_State *aState, T &&aVal) {
        if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            lua_pushboolean(aState, std::forward<T>(aVal));
        } else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
            lua_pushnumber(aState, std::forward<T>(aVal));
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            lua_pushlstring(aState, aVal.c_str(), aVal.length());
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
            lua_pushcfunction(aState, adapt(std::forward<T>(aVal)));
        }
    }

    struct IncorrectType : std::runtime_error {
        IncorrectType() : std::runtime_error("Incorrect type") {}
    };
    /*
     * Given an explicit template parameter T, pop the correctly
     * typed value from the Lua stack and return it into the C++ domain.
     * We use "if constexpr" to do compile-time dispatch in a readable way here,
     * in combination with the traits we wrote above. Another approach would be to
     * write multiple specializations of fromLua using SFINAE. However,
     * here we have better control over the compile errors.
     */
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

    /*
     * A unique specialization of gCallable will be generated for every call
     * to luabind::adapt, and will store a reference to the provided callable.
     */
    template<typename Callable, typename> std::optional<Callable> gCallable;

    /*
     * We end up using std::apply to call a callable function stored in gCallable.
     * So we have this utility to return a tuple of type std::tuple<Args...>
     * after popping the correctly typed values off the Lua stack.
     *
     * Note the fold expression, similar to fromLuaTupleHelper
     */
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

    /*
     * RetHelper is an RAII/casting helper to "deduce" a function's output type
     * by deferring the deduction until a hypothetical future "cast" to another
     * type. A RetHelper will pop off the Lua stack whenever it is casted
     * based on the cast type.
     *
     * Not to be used for Lua functions without returns (since there is no returned
     * value on the stack).
     */
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
    /*
     * Store globals in Lua, retrieve or call globals from Lua.
     * Globals can be primitives or functions.
     */
    class Lua {
    public:
        Lua() {
            fState = luaL_newstate();

            // Initialize the standard library. Not necessary, but useful
            luaL_openlibs(fState);
        }

        Lua(lua_State *aState) : fState(aState) {}

        ~Lua() {
            lua_close(fState);
        }

        /*
         * Syntactic sugar for running interpreted Lua code
         */
        Lua &operator<<(const std::string &aCodeToRun) {
            loadScript(aCodeToRun);
            return *this;
        }

        /*
         * GetGlobalHelper provides:
         * - A cast operator to return the value of a Lua global given the
         *   type of the cast and the global name.
         * - An assignment operator for storing a converted
         *   C++ value into the Lua global namespace.
         * - A call operator which assumes the referenced global is a function,
         *   marshals the function arguments, calls the function, and un-marshals
         *   the return value back to C++.
         *
         * GetGlobalHelper is used by the Lua class's operator[] to provide an
         * interface for storing and referencing Lua symbols from C++.
         */
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
            // In release builds the assert is removed, and res is unused.
            [[maybe_unused]] auto res = luaL_dostring(fState, aScript.c_str());
            assert(res == LUA_OK);
        }

        /*
         * CallSpecialHelper waits till it is either casted or destroyed to call
         * the Lua function stored in the global of name aFunctionName. It must
         * wait until one of those events to actually call the function, because
         * popping when not necessary, or neglecting to, can leave the Lua stack
         * in a corrupted state.
         */
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

    template<typename Callable, typename UniqueType>
    FuncPtr adapt(const Callable &aFunc) {
        detail::gCallable<Callable, UniqueType>.emplace(aFunc);
        return &detail::adapted<Callable, UniqueType>;
    }
}

#endif //LUABIND_LUABIND_HPP
