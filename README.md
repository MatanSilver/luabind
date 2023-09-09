# Install Dependencies
```bash
vcpkg install
```

# Build
- Set ENV variable `VCPKG_TARGET_TRIPLET` (e.g. "x64-windows")
- Set ENV variable `CMAKE_TOOLCHAIN_FILE` (e.g. "./path_to_vcpkg/scripts/buildsystems/vcpkg.cmake")
```bash
cd cmake-build-debug
cmake ..
cmake --build .
```