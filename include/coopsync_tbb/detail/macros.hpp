#pragma once

// nodiscard attribute if supported

// clang-format off
#ifndef COOPSYNC_TBB_NODISCARD
  #if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard)
      #define COOPSYNC_TBB_NODISCARD [[nodiscard]]
    #endif
  #elif defined(__cplusplus) && __cplusplus >= 201703L
    #define COOPSYNC_TBB_NODISCARD [[nodiscard]]
  #endif
  #ifndef COOPSYNC_TBB_NODISCARD
    #define COOPSYNC_TBB_NODISCARD
  #endif
#endif
// clang-format on
