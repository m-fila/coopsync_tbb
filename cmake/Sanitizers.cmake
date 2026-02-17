# Sanitizers.cmake
#
# Provides:
#   - cache variable: COOPSYNC_TBB_SANITIZER (none|address|undefined|thread)
#   - function: coopsync_tbb_apply_sanitizer(<target>)

include_guard(GLOBAL)

set(
  COOPSYNC_TBB_SANITIZER
  "none"
  CACHE STRING "Enable one sanitizer: none;address;undefined;thread")
set_property(CACHE COOPSYNC_TBB_SANITIZER PROPERTY STRINGS none address undefined thread)

function(coopsync_tbb_apply_sanitizer target_name)
  if(NOT TARGET ${target_name})
    message(FATAL_ERROR "coopsync_tbb_apply_sanitizer: target '${target_name}' does not exist")
  endif()

  if(COOPSYNC_TBB_SANITIZER STREQUAL "none")
    return()
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|GNU)$")
    message(
      FATAL_ERROR
        "COOPSYNC_TBB_SANITIZER requires Clang or GCC (got '${CMAKE_CXX_COMPILER_ID}').")
  endif()

  if(COOPSYNC_TBB_SANITIZER STREQUAL "address")
    set(_coopsync_tbb_sanitize_flag "address")
  elseif(COOPSYNC_TBB_SANITIZER STREQUAL "undefined")
    set(_coopsync_tbb_sanitize_flag "undefined")
  elseif(COOPSYNC_TBB_SANITIZER STREQUAL "thread")
    set(_coopsync_tbb_sanitize_flag "thread")
  else()
    message(
      FATAL_ERROR
        "Invalid COOPSYNC_TBB_SANITIZER='${COOPSYNC_TBB_SANITIZER}'. Use one of: none, address, undefined, thread.")
  endif()

  get_target_property(_coopsync_tbb_target_type ${target_name} TYPE)
  if(_coopsync_tbb_target_type STREQUAL "INTERFACE_LIBRARY")
    set(_coopsync_tbb_scope "INTERFACE")
  else()
    set(_coopsync_tbb_scope "PUBLIC")
  endif()

  target_compile_options(
    ${target_name}
    ${_coopsync_tbb_scope}
    -fsanitize=${_coopsync_tbb_sanitize_flag}
    -fno-omit-frame-pointer)
  target_link_options(${target_name} ${_coopsync_tbb_scope}
                      -fsanitize=${_coopsync_tbb_sanitize_flag})

  if(COOPSYNC_TBB_SANITIZER STREQUAL "undefined")
    target_compile_options(${target_name} ${_coopsync_tbb_scope}
                           -fno-sanitize-recover=all)
  endif()

  unset(_coopsync_tbb_sanitize_flag)
  unset(_coopsync_tbb_target_type)
  unset(_coopsync_tbb_scope)
endfunction()
