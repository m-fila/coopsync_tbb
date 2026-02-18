#pragma once

// nodiscard attribute if supported

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(nodiscard)
#define COOPSYNC_TOOLS_NODISCARD [[nodiscard]]
#endif
#else
#if __cplusplus > 201603L
#define COOPSYNC_TOOLS_NODISCARD [[nodiscard]]
#endif
#endif
#ifndef COOPSYNC_TOOLS_NODISCARD
#define COOPSYNC_TOOLS_NODISCARD
#endif
