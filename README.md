# CoopSync for TBB

[![tests](https://github.com/m-fila/coopsync_tbb/actions/workflows/tests.yml/badge.svg)](https://github.com/m-fila/coopsync_tbb/actions/workflows/tests.yml)
[![sanitizers](https://github.com/m-fila/coopsync_tbb/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/m-fila/coopsync_tbb/actions/workflows/sanitizers.yml)
[![](https://img.shields.io/badge/docs-dev-blue.svg)](https://m-fila.github.io/coopsync_tbb/)

Cooperative synchronization primitives for TBB resumable tasks

## Status

| Primitive            | Status |
| -------------------- | ------ |
| `barrier`            | ✔️     |
| `buffered_channel`   | ❌     |
| `unbuffered_channel` | ❌     |
| `condition_variable` | ✔️     |
| `future`             | ❌     |
| `packaged_task`      | ❌     |
| `latch`              | ✔️     |
| `mutex`              | ✔️     |
| `recursive_mutex`    | ❌     |
| `shared_mutex`       | ❌     |
| `semaphore`          | ✔️     |

✔️: supported, ❌: not supported

| Accelerators and libraries | Status |
| -------------------------- | ------ |
| CUDA                       | ✔️     |
| HIP                        | ✔️     |
| SYCL                       | ❌     |
| ONNX                       | ❌     |
| Triton                     | ❌     |

✔️: supported, ❌: not supported
