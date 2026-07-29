# Native host tests

A native C++ compiler is required.

```text
cmake -S tests/host -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```
