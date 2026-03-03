# CoopSync for TBB documentation

This project uses doxygen for documentation. To generate documentation, make sure doxygen is installed, then either use the CMake preset:

```
cmake --workflow --preset docs
```

or enable it manually:

```
cmake -S. -Bbuild -DCOOPSYNC_TBB_BUILD_DOCS=ON
cmake --build build --target coopsync_tbb_docs
```

The documentation will be then available under `./build/docs/html/`.
