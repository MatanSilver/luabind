//
// Created by Matan Silver on 5/31/23.
//

#ifndef LUABIND_TRAITS_HPP
#define LUABIND_TRAITS_HPP

#include <optional>
#include <tuple>
#include <type_traits>

namespace luabind::detail::traits {
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

    template <typename>
    struct is_vector : std::false_type{};

    template <typename T>
    struct is_vector<std::vector<T>> : std::true_type{};

    template <typename T>
    constexpr bool is_vector_v = is_vector<T>::value;


    template <typename>
    struct is_tuple : std::false_type{};

    template <typename ...Args>
    struct is_tuple<std::tuple<Args...>> : std::true_type{};

    template <typename T>
    constexpr bool is_tuple_v = is_tuple<T>::value;
}

#endif //LUABIND_TRAITS_HPP
