#pragma once

// nodiscard attribute if supported

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(nodiscard)
#define COOPSYNC_TBB_NODISCARD [[nodiscard]]
#endif
#else
#if __cplusplus > 201603L
#define COOPSYNC_TBB_NODISCARD [[nodiscard]]
#endif
#endif
#ifndef COOPSYNC_TBB_NODISCARD
#define COOPSYNC_TBB_NODISCARD
#endif
