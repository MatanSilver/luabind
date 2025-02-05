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
#include <array>
#include <ranges>
#include <algorithm>

namespace luabind::detail::traits {
/*
 * In order to dispatch to the right type conversions, we need some
 * utilities for determining compile-time information about types.
 * We'll use the "traits" namespace for this purpose.
 */

template <typename T>
inline constexpr bool always_false_v = false;

/*
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
 *
 * Only use function_traits on something you know is invocable
 */
template <typename Callable>
struct function_traits;

template <typename RetType, typename... Args>
struct function_traits<RetType (*)(Args...)> {
  using ReturnType = RetType;
  using ArgumentTypes = std::tuple<Args...>;
};

template <typename RetType, typename ClassType, typename... Args>
struct function_traits<RetType (ClassType::*)(Args...)> {
  using ReturnType = RetType;
  using ArgumentTypes = std::tuple<Args...>;
};

template <typename RetType, typename ClassType, typename... Args>
struct function_traits<RetType (ClassType::*)(Args...) const> {
  using ReturnType = RetType;
  using ArgumentTypes = std::tuple<Args...>;
};

template <typename Callable>
struct function_traits {
  using CallType = decltype(&Callable::operator());
  using ReturnType = typename function_traits<CallType>::ReturnType;
  using ArgumentTypes = typename function_traits<CallType>::ArgumentTypes;
};

struct MightCollide {
  void operator()() {}
};

struct WontCollide {};

template <typename T>
struct DetectCollision : public std::conditional_t<std::is_class_v<T>, T, WontCollide>, public MightCollide {
};

template <typename T>
constexpr bool has_call_operator = !requires() { &DetectCollision<T>::operator(); };

/*
 * Determining if something is callable is a bit complicated. You can use std::is_invocable
 * if you happen to know the types at code-authoring time, but we don't. You can detect
 * function pointers using is_function and is_member_function_pointer, but detecting objects
 * with call operators is tricky. Call operators might be templated and specialized for any
 * number of input types, and they can be inherited, etc. We use a trick involving C++20 "concepts":
 *
 * Getting the address of the operator() for a class that has multiple implementations is a compile-time
 * error. We can synthetically construct a class that inherits from two bases: one that we know has
 * a call operator, and another that we are trying to detect if it has a call operator. If compilation
 * fails, the "test" class is callable. Then, we can take advantage of "concepts" to turn that
 * compile-time error into a constraint. Composing all of these things, we can detect all of the
 * different kinds of invocable types without knowing the argument types in advance.
 */
template <typename T>
constexpr bool is_callable_v = std::is_function_v<std::remove_pointer_t<T>> ||
    std::is_member_function_pointer_v<T> ||
    has_call_operator<T>;

template <typename>
struct is_vector : std::false_type {
};

template <typename T>
struct is_vector<std::vector<T>> : std::true_type {
};

template <typename T>
constexpr bool is_vector_v = is_vector<T>::value;

template <typename>
struct is_tuple : std::false_type {
};

template <typename ...Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {
};

template <typename T>
constexpr bool is_tuple_v = is_tuple<T>::value;
}

namespace luabind::meta {
static inline constexpr size_t MAX_FIELD_SIZE = 64;

/*
 * Something that can store the data from a string at compile-time. std::string is supposed to be constexpr
 * enough in C++20, but it seems MSVC might not have implemented that yet.
 */
using DiscriminatorContainer = std::array<char, MAX_FIELD_SIZE>;

/*
 * A type that can store a value of arbitrary type, associated with a compile-time name
 */
template <DiscriminatorContainer Discriminator, typename Type>
struct field {
  using T = Type;
  T fValue;
  static inline constexpr DiscriminatorContainer fDiscriminator = Discriminator;

  template <DiscriminatorContainer TestDiscriminator>
  static constexpr bool hasName() {
    return TestDiscriminator==fDiscriminator;
  }

  template <DiscriminatorContainer OtherDiscriminator, typename OtherType>
  bool operator==(field<OtherDiscriminator, OtherType> const &aOther) const {
    return (fDiscriminator==OtherDiscriminator) && std::is_same_v<T, OtherType> && (fValue==aOther.fValue);
  }
};

/*
 * Finds the index of the field in a table that has a particular name
 * Constexpr, so this can be invoked at compile-time to reference a particular field of a table by name
 * with no runtime overhead
 */
template <typename Fields, DiscriminatorContainer TestDiscriminator, size_t ...I>
constexpr size_t getIndexMatchingName(std::index_sequence<I...>) {
  using NameEqualT = std::array<bool, std::tuple_size_v<Fields>>;
  NameEqualT isEqual{std::tuple_element_t<I, Fields>::template hasName<TestDiscriminator>() ...};
  auto found = std::find(isEqual.cbegin(), isEqual.cend(), true);
  return found - isEqual.cbegin();
}

/*
 * A struct-like type that stores fields of arbitrary types along with associated field names
 * Allows name-based addressing of the stored fields
 */
template <typename ...MetaFields>
struct table {
  using Fields = std::tuple<MetaFields...>;
  Fields fFields;

  table(MetaFields ...aFields) : fFields{aFields...} {} // NOLINT(google-explicit-constructor)

  table(table<MetaFields...> const &aOther) : fFields{aOther.fFields} {};

  template <DiscriminatorContainer Discriminator>
  auto &f() {
    constexpr size_t
        idx = getIndexMatchingName<Fields, Discriminator>(std::make_index_sequence<std::tuple_size_v<Fields>>());
    static_assert(idx < std::tuple_size_v<Fields>, "Field not found");
    return std::get<idx>(fFields).fValue;
  }

  bool operator==(table<MetaFields...> const &aOther) const {
    return fFields==aOther.fFields;
  }
};

namespace literals {
constexpr DiscriminatorContainer operator ""_f(const char *aStr, const unsigned int aSize) {
  DiscriminatorContainer res{};
  std::ranges::copy_n(aStr, std::min(aSize, res.max_size()), res.begin());
  return res;
}
}
}

namespace luabind::detail::traits {
template <typename>
struct is_table : std::false_type {};

template <typename ...Args>
struct is_table<meta::table<Args...>> : std::true_type {};

template <typename T>
constexpr bool is_table_v = is_table<T>::value;
}

namespace luabind {
/*
 * luabind::adapt is a clever function that converts a callable argument
 * to a c-style function pointer to a function that adheres to the Lua
 * requirements of taking in a lua_State* and returning an int. The way we do
 * this is by creating a new specialization of a global variable /for every call site/
 * of luabind::adapt, and storing a reference to the callable in that specialized global
 * variable. The actual implementation of the pointed-to function is a specialization of
 * luabind::detail::adapted associated with the luabind::adapt call site, which uses
 * compile-time information about the argument and return types of the callable object
 * to serialize and deserialize those values between C++ and Lua domains.
 *
 * The synthetic lua_CFunction-style function must maintain a long-lasting reference to the
 * underlying callable passed to luabind::adapt. However, with a strictly defined
 * interface, said reference can't be exposed to the function through an argument.
 * The trick we use to avoid this is to create a templated global (luabind::detail::gCallable)
 * with a "UniqueType" template parameter with a default value equal to the type of
 * an empty lambda. Distinct lambdas are guaranteed to have distinct types. The result
 * is we can implicitly create a new global for every syntactic call to luabind::adapt!
 * Thus, every call to luabind::adapt can store a unique reference to the callable it is
 * provided with, for the luabind::detail::adapted specialization to call when needed.
 */
template <typename Callable, typename UniqueType = decltype([]() {})>
lua_CFunction adapt(const Callable &aFunc);

struct RuntimeError : std::runtime_error {
  explicit RuntimeError(std::string const &aSubMsg) : std::runtime_error("Lua runtime error: " + aSubMsg) {}
};

struct MemoryError : std::runtime_error {
  explicit MemoryError(std::string const &aSubMsg) : std::runtime_error("Lua memory error: " + aSubMsg) {}
};

struct ErrorHandlerError : std::runtime_error {
  explicit ErrorHandlerError(std::string const &aSubMsg) : std::runtime_error("Lua error handler error: " + aSubMsg) {}
};

struct SyntaxError : std::runtime_error {
  explicit SyntaxError(std::string const &aSubMsg) : std::runtime_error("Lua syntax error: " + aSubMsg) {}
};

struct FileError : std::runtime_error {
  explicit FileError(std::string const &aSubMsg) : std::runtime_error("Lua file error: " + aSubMsg) {}
};

struct IncorrectType : std::runtime_error {
  explicit IncorrectType(std::string const &aSubMsg) : std::runtime_error("Incorrect type: " + aSubMsg) {}
};
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
template <typename T>
void toLua(lua_State *aState, T const &aVal);

/*
 * luabind::detail::fromLua receives a lua_State, and based on the template
 * type T will pop a value off the Lua stack and return it in the C++ domain.
 * Callers must provide the type T because it is not associated with an input
 * argument.
 */
template <typename T>
T fromLua(lua_State *aState);

template <typename T>
void setTableElement(lua_State *aState, T const &aVal, int aIdx) {
  // First push the Lua-domain converted aVal onto the stack
  toLua(aState, aVal);

  // idx -1 on the stack is the converted aVal, -2 is the table
  lua_seti(aState, -2, aIdx);
}

template <typename T>
T getTableElement(lua_State *aState, int aIdx) {
  lua_geti(aState, -1, aIdx);
  return fromLua<T>(aState);
}

/*
 * setTableElementsFromTuple uses a fold-expression along with a compile-time
 * index_sequence to expand a single syntactic call to setTableElement
 * to N calls, one for each tuple element.
 */
template <typename ...Args, std::size_t... I>
void setTableElementsFromTuple(lua_State *aState, const std::tuple<Args...> &aVal, std::index_sequence<I...>) {
  (setTableElement(aState, std::get<I>(aVal), I + 1), ...);
}

template <typename T>
void toLuaTuple(lua_State *aState, const T &aVal) {
  lua_createtable(aState, std::tuple_size_v<std::decay_t<T>>, 0);
  setTableElementsFromTuple(aState, aVal, std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>());
}

/*
 * getTableElementsAsTuple uses a pack expansion along with a compile-time
 * index_sequence to expand a single syntactic call to getTableElement
 * to N calls, one for each tuple element.
 *
 * T is assumed to be a tuple type, and we use a pack expansion within
 * an initializer list to construct the return tuple in-place.
*/
template <typename T, std::size_t... I>
T getTableElementsAsTuple(lua_State *aState, std::index_sequence<I...>) {
  return {getTableElement<std::tuple_element_t<I, T>>(aState, I + 1) ...};
}

template <typename T>
T fromLuaTuple(lua_State *aState) {
  constexpr std::size_t tupleSize = std::tuple_size_v<T>;
  return getTableElementsAsTuple<T>(aState, std::make_index_sequence<tupleSize>());
}

template <typename T, size_t ...I>
T getTableElementsAsTable(lua_State *aState, std::index_sequence<I...>) {
  // Avoid another template function with an immediately invoked lambda to satisfy pack expansion
  return {{[aState]() {
    lua_getfield(aState, -1, std::tuple_element_t<I, typename T::Fields>::fDiscriminator.data());
    return fromLua<typename std::tuple_element_t<I, typename T::Fields>::T>(aState);
  }()}...};
}

template <typename T, size_t ...I>
void setTableElementsAsTable(lua_State *aState, const T &aTable, std::index_sequence<I...>) {
  // Avoid another template function with an immediately invoked lambda to satisfy fold expression
  ([aState, &aTable]() {
    toLua(aState, std::get<I>(aTable.fFields).fValue);

    // idx -1 on the stack is the converted aVal, -2 is the table
    lua_setfield(aState, -2, std::tuple_element_t<I, typename T::Fields>::fDiscriminator.data());
  }(), ...);
}

template <typename T>
T fromLuaTable(lua_State *aState) {
  static_assert(traits::is_table_v<T>);

  // For each field according to the index_sequence, deserialize that element by indexing into the lua tuple
  // using the field name, and calling fromLua using the field type
  return getTableElementsAsTable<T>(aState, std::make_index_sequence<std::tuple_size_v<typename T::Fields>>());
}

template <typename T>
void toLuaTable(lua_State *aState, const T &aVal) {
  static_assert(traits::is_table_v<T>);

  // For each field according to the index_sequence, serialize that element by calling toLua based on
  // the field type
  lua_createtable(aState, std::tuple_size_v<std::decay_t<typename T::Fields>>, 0);
  setTableElementsAsTable(aState, aVal, std::make_index_sequence<std::tuple_size_v<typename T::Fields>>());
}

enum class ScopeTrigger {
  SUCCESS,
  FAILURE,
  ALWAYS
};

template <ScopeTrigger Trigger, std::invocable T>
class ScopeGuard {
  public:
  [[nodiscard]] explicit ScopeGuard(T aInvocable) : fInvocable{aInvocable} {}

  ~ScopeGuard() {
    if constexpr (Trigger==ScopeTrigger::SUCCESS) {
      if (!std::uncaught_exceptions()) {
        fInvocable();
      }
    } else if constexpr (Trigger==ScopeTrigger::FAILURE) {
      if (std::uncaught_exceptions()) {
        fInvocable();
      }
    } else if constexpr (Trigger==ScopeTrigger::ALWAYS) {
      fInvocable();
    } else {
      static_assert(detail::traits::always_false_v<T>, "Unhandled scope trigger");
    }
  }

  private:
  T fInvocable;
};

/*
 * CTAD does not support partial explicit template specification
 * which is needed to specify the non-type template parameter
 * trigger. However, this is permitted in a normal function,
 * so we can get a nice API by writing a construction wrapper
 * around ScopeGuard
 */
template <ScopeTrigger Trigger = ScopeTrigger::ALWAYS, std::invocable T>
auto makeScopeGuard(T aInvocable) {
  return ScopeGuard<Trigger, T>(aInvocable);
}

/*
 * Given a value aVal with deduced type T, push the correctly
 * typed value onto the Lua stack. We use "if constexpr" to do
 * compile-time dispatch in a readable way here, in combination
 * with the traits we wrote above. Another approach would be to
 * write multiple specializations of toLua using SFINAE. However,
 * here we have better control over the compile errors.
 */
template <typename T>
void toLua(lua_State *aState, T const &aVal) {
  int initialStackSize = lua_gettop(aState);
  auto guard = makeScopeGuard([aState, initialStackSize]() {
    int expectedStackSize = initialStackSize;
    if (!std::uncaught_exceptions()) {
      expectedStackSize++;
    }
    assert(lua_gettop(aState)==expectedStackSize);
  });
  if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
    lua_pushboolean(aState, aVal);
  } else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
    lua_pushnumber(aState, aVal);
  } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
    lua_pushlstring(aState, aVal.c_str(), aVal.length());
  } else if constexpr (std::is_same_v<std::decay_t<T>, char *> || std::is_same_v<std::decay_t<T>, const char *>) {
    lua_pushstring(aState, aVal);
  } else if constexpr (traits::is_vector_v<std::decay_t<T>>) {
    lua_createtable(aState, aVal.size(), 0);
    for (int i = 0; i < aVal.size(); ++i) {
      setTableElement(aState, aVal[i], i + 1);
    }
  } else if constexpr (traits::is_tuple_v<std::decay_t<T>>) {
    toLuaTuple(aState, aVal);
  } else if constexpr (traits::is_callable_v<std::decay_t<T>>) {
    lua_pushcfunction(aState, adapt(aVal));
  } else if constexpr (traits::is_table_v<std::decay_t<T>>) {
    toLuaTable(aState, aVal);
  } else {
    static_assert(traits::always_false_v<T>, "Unsupported type");
  }
}

/*
 * Given an explicit template parameter T, pop the correctly
 * typed value from the Lua stack and return it into the C++ domain.
 * We use "if constexpr" to do compile-time dispatch in a readable way here,
 * in combination with the traits we wrote above. Another approach would be to
 * write multiple specializations of fromLua using SFINAE. However,
 * here we have better control over the compile errors.
 */
template <typename T>
T fromLua(lua_State *aState) {
  int initialStackSize = lua_gettop(aState);
  auto guard = makeScopeGuard([aState, initialStackSize]() {
    int expectedStackSize = initialStackSize;
    if (!std::uncaught_exceptions()) {
      expectedStackSize--;
    }
    assert(lua_gettop(aState)==expectedStackSize);
  });
  auto popOnExit = makeScopeGuard<ScopeTrigger::SUCCESS>([aState]() { lua_pop(aState, 1); });
  if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
    if (!lua_isboolean(aState, -1)) {
      throw IncorrectType("Runtime type cannot be converted to bool");
    }
    T ret = lua_toboolean(aState, -1);
    return ret;
  } else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
    if (!lua_isnumber(aState, -1)) {
      throw IncorrectType("Runtime type cannot be converted to an arithmetic type");
    }
    T ret = lua_tonumber(aState, -1);
    return ret;
  } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
    if (!lua_isstring(aState, -1) || lua_isnumber(aState, -1)) {
      // lua_isstring returns true for numbers, oddly
      throw IncorrectType("Runtime type cannot be converted to a string");
    }
    T ret = lua_tostring(aState, -1);
    return ret;
    // We don't support const char* for memory safety reasons
  } else if constexpr (traits::is_vector_v<std::decay_t<T>>) {
    if (!lua_istable(aState, -1)) {
      throw IncorrectType("Runtime type cannot be converted to a vector");
    }
    T retVec;
    for (int i = 0; i < lua_rawlen(aState, -1); ++i) {
      lua_geti(aState, -1, i + 1);
      retVec.emplace_back(fromLua<typename T::value_type>(aState));
    }
    return retVec;
  } else if constexpr (traits::is_tuple_v<std::decay_t<T>>) {
    auto newTuple = fromLuaTuple<std::decay_t<T>>(aState);
    return newTuple;
  } else if constexpr (traits::is_callable_v<std::decay_t<T>>) {
    static_assert(detail::traits::always_false_v<T>,
                  "Unable to create a function object from Lua, use the GetGlobalHelper/CallHelper instead");
  } else if constexpr (traits::is_table_v<std::decay_t<T>>) {
    auto newTable = fromLuaTable<std::decay_t<T>>(aState);
    return newTable;
  } else {
    static_assert(detail::traits::always_false_v<T>, "Unsupported type");
  }
}

/*
 * A unique specialization of gCallable will be generated for every call
 * to luabind::adapt, and will store a reference to the provided callable.
 */
template <typename Callable, typename> std::optional<Callable> gCallable;

/*
 * We end up using std::apply to call a callable function stored in gCallable.
 * So we have this utility to return a tuple of type std::tuple<Args...>
 * after popping the correctly typed values off the Lua stack.
 *
 * Note the pack expansion, similar to getTableElementsAsTuple
 */
template <typename ...Args>
std::tuple<Args...> getArgsAsTuple(lua_State *aState, std::tuple<Args...>) {
  // Args... may be empty if there are on args, which is ok despite style warning about the empty decl
  return {detail::fromLua<Args>(aState) ...};
};

template <typename Callable, typename UniqueType>
int adapted(lua_State *aState) {
  using RetType = typename detail::traits::function_traits<Callable>::ReturnType;
  using ArgTypes = typename detail::traits::function_traits<Callable>::ArgumentTypes;
  try {
    if constexpr (std::is_same_v<RetType, void>) {
      std::apply(*gCallable<Callable, UniqueType>, getArgsAsTuple(aState, ArgTypes()));
      return 0;
    } else {
      toLua(aState, std::apply(*gCallable<Callable, UniqueType>, getArgsAsTuple(aState, ArgTypes())));
      return 1;
    }
  } catch (std::exception &e) {
    luaL_error(aState, e.what());
    return 0;
  }
}
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
    fOwnsState = true;

    // Initialize the standard library. Not necessary, but useful
    luaL_openlibs(fState);
  }

  explicit Lua(lua_State *aState) : fState(aState), fOwnsState(false) {}

  Lua(Lua const&) = delete; // Copy assignment

  Lua& operator=(Lua const&) = delete; // Copy constructor

  Lua(Lua&& aOther) : fState(aOther.fState), fOwnsState(aOther.fOwnsState) { // Move constructor
    aOther.fState = nullptr;
    aOther.fOwnsState = false;
  }

  Lua& operator=(Lua&& aOther) { // Move assignment
    std::swap(this->fState, aOther.fState);
    this->fOwnsState = aOther.fOwnsState;
    return *this;
  }

  ~Lua() {
    if (fOwnsState && fState != nullptr) {
      lua_close(fState);
    }
  }

  /*
   * Syntactic sugar for running interpreted Lua code
   */
  Lua &operator<<(const std::string_view aCodeToRun) {
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
    GetGlobalHelper(const std::string_view aGlobalName, Lua &aLua)
        : fGlobalName(aGlobalName), fLua(aLua) {}

    template <typename ...Args>
    auto operator()(const Args &... aArgs) {
      return fLua.call(fGlobalName, aArgs...);
    }

    template <typename T>
    operator T() { // NOLINT(google-explicit-constructor)
      lua_getglobal(fLua.fState, fGlobalName.data());
      return detail::fromLua<T>(fLua.fState);
    }

    template <typename T>
    Lua &operator=(T const &aVal) { // NOLINT(misc-unconventional-assign-operator)
      detail::toLua(fLua.fState, aVal);
      lua_setglobal(fLua.fState, fGlobalName.data());
      return fLua;
    }

    private:
    const std::string_view fGlobalName;
    Lua &fLua;
  };

  /*
   * Gets the global of the name aGlobalName
   */
  auto operator[](const std::string_view aGlobalName) {
    return GetGlobalHelper{aGlobalName, *this};
  }

  private:
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
    explicit RetHelper(lua_State *aState) : fState(aState), fWasCasted(false) {}

    ~RetHelper() {
      // If we never popped off the stack, assumptions about
      // the stack size may be incorrect
      assert(fWasCasted);
    }

    template <typename T>
    operator T() { // NOLINT(google-explicit-constructor)
      assert(!fWasCasted); // We can only pop off the stack once
      fWasCasted = true;
      return detail::fromLua<T>(fState);
    }

    private:
    lua_State *fState;
    bool fWasCasted;
  };

  template <typename ...Args>
  void pushFunctionAndArgs(const std::string_view aFunctionName, const Args &... aArgs) {
    using namespace std::string_literals;
    // push function on stack
    lua_getglobal(fState, aFunctionName.data());

    if (!lua_isfunction(fState, -1) && !lua_iscfunction(fState, -1)) {
      lua_pop(fState, 1); // We need to clean up the global we retrieved before throwing

      throw RuntimeError("Global by name "s + aFunctionName.data() + " is not a function"s);
    }
    // push aArgs on stack, in order left to right
    (detail::toLua(fState, aArgs), ...);
  }

  void handleLuaErrCode(int aErrCode) {
    using namespace std::string_literals;
    auto stringFromErrorOnStack = [&]() -> std::string {
      return detail::fromLua<std::string>(fState);
    };
    switch (aErrCode) {
      case LUA_OK:break;
      case LUA_ERRRUN:throw RuntimeError(stringFromErrorOnStack());
      case LUA_ERRMEM:throw MemoryError(stringFromErrorOnStack());
      case LUA_ERRERR:throw ErrorHandlerError(stringFromErrorOnStack());
      case LUA_ERRSYNTAX:throw SyntaxError(stringFromErrorOnStack());
      case LUA_ERRFILE:throw FileError(stringFromErrorOnStack());
      default:throw RuntimeError("Unknown error code: "s + std::to_string(aErrCode));
    }
  }

  template <typename ...Args>
  void callWithoutReturnValue(const std::string_view aFunctionName, const Args &... aArgs) {
    pushFunctionAndArgs(aFunctionName, aArgs...);
    auto errCode = lua_pcall(fState, sizeof...(aArgs), 0, 0);
    handleLuaErrCode(errCode);
  }

  template <typename ...Args>
  auto callWithReturnValue(const std::string_view aFunctionName, const Args &... aArgs) {
    pushFunctionAndArgs(aFunctionName, aArgs...);
    auto errCode = lua_pcall(fState, sizeof...(aArgs), 1, 0);
    handleLuaErrCode(errCode);
    return RetHelper(fState);
  }

  void loadScript(const std::string_view aScript) {
    auto res = luaL_loadstring(fState, aScript.data());
    handleLuaErrCode(res);
    res = lua_pcall(fState, 0, LUA_MULTRET, 0);
    handleLuaErrCode(res);
  }

  /*
   * CallHelper waits till it is either casted or destroyed to call
   * the Lua function stored in the global of name aFunctionName. It must
   * wait until one of those events to actually call the function, because
   * popping when not necessary, or neglecting to, can leave the Lua stack
   * in a corrupted state.
   */
  template <typename ...Args>
  struct CallHelper {
    CallHelper(Lua &aLua, const std::string_view aFunctionName, const std::tuple<Args...> &aArgs)
        : fLua(aLua), fFunctionName(aFunctionName), fArgs{aArgs}, fWasCasted(false) {}

    ~CallHelper() {
      // If we never called the operator T(), we should run the function with no return val
      if (!fWasCasted) {
        auto lam = [&](auto const &... aArgs) { fLua.callWithoutReturnValue(aArgs...); };
        std::apply(lam, std::tuple_cat(std::tuple{fFunctionName}, fArgs));
      }
    }

    template <typename T>
    operator T() { // NOLINT(google-explicit-constructor)
      fWasCasted = true;
      auto lam = [&](auto const &... aArgs) { return fLua.callWithReturnValue(aArgs...); };
      return std::apply(lam, std::tuple_cat(std::tuple{fFunctionName}, fArgs));
    }

    private:
    Lua &fLua;
    const std::string_view fFunctionName;
    const std::tuple<Args...> fArgs;
    bool fWasCasted;
  };

  template <typename ...Args>
  auto call(const std::string_view aFunctionName, const Args &... aArgs) {
    return CallHelper{*this, aFunctionName, std::tuple{aArgs...}};
  }

  private:
  lua_State *fState;
  bool fOwnsState;
};

template <typename Callable, typename UniqueType>
lua_CFunction adapt(const Callable &aFunc) {
  detail::gCallable<Callable, UniqueType>.emplace(aFunc);
  return &detail::adapted<Callable, UniqueType>;
}
}

#endif //LUABIND_LUABIND_HPP
