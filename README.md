# CoopSync for TBB

Cooperative synchronization primitives for TBB resumable tasks

## Status

| Primitive            | Status |
| -------------------- | ------ |
| `barrier`            | ❌     |
| `buffered_channel`   | ❌     |
| `unbuffered_channel` | ❌     |
| `condition_variable` | ❌     |
| `future`             | ❌     |
| `packaged_task`      | ❌     |
| `latch`              | ✔️     |
| `mutex`              | ✔️     |
| `recursive_mutex`    | ❌     |
| `shared_mutex`       | ❌     |
| `semaphore`          | ❌     |

✔️: supported, ❌: not supported

| Accelerators and libraries | Status |
| -------------------------- | ------ |
| CUDA                       | ✔️     |
| HIP                        | ✔️     |
| SYCL                       | ❌     |
| ONNX                       | ❌     |
| Triton                     | ❌     |

✔️: supported, ❌: not supported
