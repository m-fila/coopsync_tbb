# CoopSync for TBB downstream example

This directory is a minimal consumer project that verifies that an *installed* `CoopSync_TBB` can be found via CMake and linked.

After installing `CoopSync_TBB`, run the following command(s) from the repository's root:

```bash
# assuming installation in ./install/
cd cmake/downstream && cmake --workflow --preset downstream

# or step-by-step:
cmake -S cmake/downstream --preset downstream -DCMAKE_PREFIX_PATH=../../install
cmake --build build/downstream
ctest --test-dir build/downstream
```
