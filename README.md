# CoopSync for TBB

[![tests](https://github.com/m-fila/coopsync_tbb/actions/workflows/tests.yml/badge.svg)](https://github.com/m-fila/coopsync_tbb/actions/workflows/tests.yml)
[![sanitizers](https://github.com/m-fila/coopsync_tbb/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/m-fila/coopsync_tbb/actions/workflows/sanitizers.yml)
[![](https://img.shields.io/badge/docs-dev-blue.svg)](https://m-fila.github.io/coopsync_tbb/)

Cooperative synchronization primitives for TBB resumable tasks

## Features

This library provides a set of cooperative synchronization primitives designed for use with oneTBB tasks. Unlike standard primitives that block the CPU, these primitives suspend the current task, allowing other tasks to execute while waiting.

The interfaces of the primitives are modelled after the C++ standard library wherever there is a direct correspondence.

| Primitive            | Status |
| -------------------- | ------ |
| `barrier`            | ✔️     |
| `buffered_channel`   | ❌     |
| `unbuffered_channel` | ❌     |
| `condition_variable` | ✔️     |
| `future`             | ✔️     |
| `packaged_task`      | ✔️     |
| `latch`              | ✔️     |
| `mutex`              | ✔️     |
| `recursive_mutex`    | ❌     |
| `shared_mutex`       | ✔️     |
| `semaphore`          | ✔️     |

✔️: supported, ❌: not implemented

The project also provides optional integrations for GPU and other libraries. These integrations allow the current task to be suspended until the associated asynchronous operations complete.

| Accelerators and libraries | Status |
| -------------------------- | ------ |
| CUDA                       | ✔️     |
| HIP                        | ✔️     |
| SYCL                       | ❌     |
| ONNX                       | ❌     |
| Triton                     | ❌     |

✔️: supported, ❌: not implemented

## Requirements

Build requirements:

- C++17 or later compiler
- oneTBB v2021.8 or later

Optional dependencies (only required for integration headers):

- CUDA v10 or later
- HIP v5 or later

The optional dependencies are not needed to build the core library. They are only required if you include headers that provide given integration, such as `coopsync/cuda/wait_for.hpp` for CUDA or the corresponding HIP headers.

## Building

This project uses CMake. To configure and build the library with default settings, run the following:

```sh
cmake --preset default
cmake --build --preset default
```

By default the project will fetch the dependencies. To find and use the system dependencies instead, set the CMake ` -DCOOPSYNC_TBB_USE_SYSTEM_LIBS=ON` flag during configuration.

## Using in a CMake project

This project installs a CMake configuration and exports a CMake target, making it easy to use in other projects. The library can be located with `find_package` and linked to other targets:

```cmake
find_package(CoopSync_TBB REQUIRED)

target_link_libraries(your_target PUBLIC CoopSync_TBB::CoopSync_TBB)
```

## Disclaimer

CoopSync for TBB is an independent extension library designed to work with oneTBB.

This project is not affiliated with, endorsed by, sponsored by, or otherwise associated with Intel Corporation, UXL Foundation or the oneTBB project.
