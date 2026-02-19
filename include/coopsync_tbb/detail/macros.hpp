#pragma once

// nodiscard attribute if supported

// clang-format off
#ifdef __has_cpp_attribute
  #if __has_cpp_attribute(nodiscard)
    #define COOPSYNC_TBB_NODISCARD [[nodiscard]]
  #else
    #define COOPSYNC_TBB_NODISCARD
  #endif
#elif __cplusplus > 201703L
  #define COOPSYNC_TBB_NODISCARD [[nodiscard]]
#else
  #define COOPSYNC_TBB_NODISCARD
#endif
// clang-format on
