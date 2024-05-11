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
## Function bindings:
```C++
    luabind::Lua lua;
    lua["timesTwo"] = [](int x) -> int {
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
## Different types:
```C++
    luabind::Lua lua;
    // We can even have tuples containing tuples!
    using T = std::tuple<std::string,
            int,
            bool,
            std::tuple<int, bool>>;
    T initialTuple{"thing", 1, true, {4, false}};
    lua["newtuple"] = initialTuple;
    T reconstructedTuple = lua["newtuple"];
    ASSERT_EQ(initialTuple, reconstructedTuple);
```