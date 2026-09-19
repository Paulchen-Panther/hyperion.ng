#.rst:
# FindLibDrmTap
# -------------
# Finds the libdrmtap library (https://github.com/fxd0h/libdrmtap)
#
# libdrmtap is an optional, third-party DRM/KMS screen capture library. When it
# is present, the DRM grabber uses it to capture GPU scanouts (e.g. tiled or
# compressed Intel/AMD/Nvidia framebuffers) that its built-in capture backend
# is not able to decode on its own. The grabber remains fully functional
# without this library.
#
# This will define the following variables::
#
# LIBDRMTAP_FOUND - system has libdrmtap
# LIBDRMTAP_INCLUDE_DIRS - the libdrmtap include directory
# LIBDRMTAP_LIBRARIES - the libdrmtap libraries
# LIBDRMTAP_VERSION - the version of libdrmtap
#
# and the following imported targets::
#
#   LibDrmTap::LibDrmTap   - The libdrmtap library

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LIBDRMTAP libdrmtap QUIET)
endif()

find_path(LIBDRMTAP_INCLUDE_DIR NAMES drmtap.h
                                PATHS ${PC_LIBDRMTAP_INCLUDEDIR})
find_library(LIBDRMTAP_LIBRARY NAMES drmtap
                                PATHS ${PC_LIBDRMTAP_LIBDIR})

set(LIBDRMTAP_VERSION ${PC_LIBDRMTAP_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibDrmTap
  REQUIRED_VARS
    LIBDRMTAP_LIBRARY
    LIBDRMTAP_INCLUDE_DIR
  VERSION_VAR
    LIBDRMTAP_VERSION
)

if(LIBDRMTAP_FOUND)
  set(LIBDRMTAP_LIBRARIES ${LIBDRMTAP_LIBRARY})
  set(LIBDRMTAP_INCLUDE_DIRS ${LIBDRMTAP_INCLUDE_DIR})

  if(NOT TARGET LibDrmTap::LibDrmTap)
    add_library(LibDrmTap::LibDrmTap UNKNOWN IMPORTED)
    set_target_properties(LibDrmTap::LibDrmTap PROPERTIES
                                   IMPORTED_LOCATION "${LIBDRMTAP_LIBRARY}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${LIBDRMTAP_INCLUDE_DIR}")
  endif()
endif()

mark_as_advanced(LIBDRMTAP_INCLUDE_DIR LIBDRMTAP_LIBRARY)
