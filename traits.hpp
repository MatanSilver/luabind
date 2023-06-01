//
// Created by Matan Silver on 5/31/23.
//

#ifndef LUAWRAPPER_TRAITS_HPP
#define LUAWRAPPER_TRAITS_HPP

#include <optional>
#include <tuple>
#include <type_traits>

namespace luawrapper::detail::traits {
    template<typename T>
    inline constexpr bool always_false_v = false;

    template <typename Callable>
    struct function_traits;

    template <typename ReturnType, typename... Args>
    struct function_traits<ReturnType (*)(Args...)> {
        using return_type = ReturnType;
        using argument_types = std::tuple<Args...>;
    };

    template <typename ReturnType, typename ClassType, typename... Args>
    struct  function_traits<ReturnType (ClassType::*)(Args...)> {
        using return_type = ReturnType;
        using argument_types = std::tuple<Args...>;
    };

    template <typename ReturnType, typename ClassType, typename... Args>
    struct function_traits<ReturnType (ClassType::*)(Args...) const> {
        using return_type = ReturnType;
        using argument_types = std::tuple<Args...>;
    };

    template <typename Callable>
    struct function_traits {
    private:
        using call_type = decltype(&Callable::operator());
    public:
        using return_type = typename function_traits<call_type>::return_type;
        using argument_types = typename function_traits<call_type>::argument_types;
    };

    template <typename T>
    const char* type_to_string() {
        return __PRETTY_FUNCTION__;
    }
}

#endif //LUAWRAPPER_TRAITS_HPP
