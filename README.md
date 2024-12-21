# Intro

Luabind is a simple Lua <---> C++ binding library built around template type deduction.
The basic principle is that one can embed a lua interpreter in a C++ application,
and use the functionality in this library to facilitate communication and translation
between the two language domains. There is a straightforward mapping of types:

| C++              | Lua      |
|------------------|----------|
| bool             | boolean  |
| numeric          | number   |
| char             | string   |
| std::string      | string   |
| const char *     | string   |
| std::vector      | table    |
| std::tuple       | table    |
| meta_struct      | table    |
| callable object  | function |
| function pointer | function |
| method pointer   | function |

Variables can be easily introduced into the global namespace:

```C++
luabind::Lua lua;
    lua["timesTwo"] = 1;
```

Global variables can also be read in a similar way.
Arbitrary lua code can be executed on the interpreter state through the shift operator:

```C++
luabind::Lua lua;
lua << R"(
            print("thing")
        )";
```

Right now luabind does not support an escape hatch for operating on arbitrary tables.
Either the table must have the type of all elements (and the number of elements) known,
or the table must be convertible to a vector of uniform typed values.
If a C++ function bound to lua throws an exception, this may crash the program.
Some nominal type-checking is performed when marshalling values between domains.

# Install Dependencies

```bash
vcpkg install
```

# Build

- Set ENV variable `VCPKG_TARGET_TRIPLET` (e.g. "x64-windows")
- Set ENV variable `CMAKE_TOOLCHAIN_FILE` (e.g. "./path_to_vcpkg/scripts/buildsystems/vcpkg.cmake")

```bash
cd test/cmake-build-debug
cmake ..
cmake --build .
```

# Example Usage

## Function Bindings

```C++
luabind::Lua lua;
lua["timesTwo"] =[](int x) -> int {
return x * 2;
};
lua << R"(
            callIntoCFunc = function(a)
                return timesTwo(a)
            end
        )";
int x = lua["callIntoCFunc"](4);
ASSERT_EQ(x, 8);
```

## Supported Types

```C++
luabind::Lua lua;
// We can even have tuples containing tuples!
using T = std::tuple<std::string, int, bool, std::tuple<int, bool>>;
T initialTuple{ "thing", 1, true, { 4, false }};
lua["newtuple"] = initialTuple;
T reconstructedTuple = lua["newtuple"];
ASSERT_EQ(initialTuple, reconstructedTuple);
```

C++20 does not have compile-time reflection. This means you can't know the names
of struct fields at compile-time. That would have been a nice way to make tables
with named fields. Instead, we can invent a "meta_struct" that uses clever
compile-time processing to store field names at compile-time along with the other
type information for the table elements. The end-result is similar to reflection:

```C++
using namespace luabind::meta::literals;
using namespace luabind::meta;

meta_struct<
  meta_field<"biz"_f, int>,
  meta_field<"buz"_f, bool>> bar{
    {1}, {false}
};

auto x = bar.f<"biz"_f>();
auto y = bar.f<"buz"_f>();

ASSERT_EQ(x, 1);
ASSERT_EQ(y, false);

bar.f<"buz"_f>() = true;
ASSERT_EQ(bar.f<"buz"_f>(), true);

bar.f<"biz"_f>() = 10;
ASSERT_EQ(bar.f<"biz"_f>(), 10);
```

# Compiling a Lua Module

You can also use utilities from this library to implement a C library that can be "require"-d by a lua interpreter. See
the examples/module directory for an example. The Dockerfile documents the build process and can be built with:

```
docker build -f examples/module/Dockerfile -t luabind_module .
```

# Error Handling

Through the use of `lua_pcall` and try-catch in C++, errors are bubbled up between the
C++ and Lua stacks. Lua errors are strings and get converted to runtime errors in C++
if one occurs while calling a Lua function in C++. Likewise, if a C++ exception reaches
the Lua interpreter, it will get converted to a Lua error using `luaL_error`. We try to
call into Lua in a safe way so if an exception is caught in C++, the lua interpreter is
still in a valid state and can continue operation.