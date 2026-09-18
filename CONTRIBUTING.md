# Contributing

## General considerations

### Build configuration

### Formatting and linting

This project uses pre-commit hooks to enforce code style, formatting and linting. Please ensure that you have the pre-commit hooks installed and run `pre-commit` before committing your changes. The `cppcheck` and `clang-tidy` relay on the local installation of `cppcheck` and `clang-tidy`. Please ensure that they are installed and available in your `PATH`. `cpp-check` and `clang-tidy` expect the `compile_commands.json` file to be present in the build directory. Please ensure that you have generated it by running CMake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` or using CMake presets.

## Adding new synchronization primitives

Conventions:
- [ ] `snake_case` is used for all public and non-public interfaces.
- [ ] The naming and interfaces should be modelled after `std` synchronization primitives or the library they correspond to.
- [ ] All public interfaces should be documented using Doxygen comments. Non-trivial non-public methods should also be documented.
- [ ] The library is supposed to be C++11 compatible. If any new features are essential for the new interfaces, they should be documented and hidden behind a feature test macro in `coopsync_tbb/feature_test.hpp`.

File organization:

- [ ] This project uses `.hpp`, `.cpp` and `.cppm` file extensions for C++ files.
- [ ] Add a new header file in `include/coopsync_tbb/` and a new source file in `src/` with the same name. The name should follow the name of the synchronization primitive you are adding. If multiple related classes are added it is reasonable to put them together in a single header and source file, e.g. `my_future` and `my_promise` can be added in `my_future.h` and `my_future.cpp`. The source file should include the header file and be created even if otherwise it would be empty (e.g. all the contents are templates).
- [ ] Add a new test for the feature in `tests/` named the same as the source file but with a `_test` suffix. For example, if you add `my_mutex.cpp`, add `tests/my_mutex_test.cpp`.
- [ ] Add the new header file to includes in `include/coopsync_tbb/coopsync_tbb.h`.
- [ ] Export the new classes in the `coopsync_tbb` module in `include/coopsync_tbb/src/coopsync_tbb.cppm`.

## Adding new integrations

