# FindProjectM.cmake
# Find the libprojectM library
#
# This module defines:
#   PROJECTM_FOUND        - True if libprojectM was found
#   PROJECTM_INCLUDE_DIRS - Include directories
#   PROJECTM_LIBRARIES    - Libraries to link against
#   PROJECTM_VERSION      - Version string (if available)
#
# It also creates an imported target:
#   projectM::projectM

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(PC_PROJECTM QUIET libprojectM4)
endif()

find_path(PROJECTM_INCLUDE_DIR
	NAMES projectM-4/projectM.h
	HINTS ${PC_PROJECTM_INCLUDEDIR} ${PC_PROJECTM_INCLUDE_DIRS}
	PATH_SUFFIXES projectM-4
)

find_library(PROJECTM_LIBRARY
	NAMES projectM-4 projectM
	HINTS ${PC_PROJECTM_LIBDIR} ${PC_PROJECTM_LIBRARY_DIRS}
)

set(PROJECTM_VERSION ${PC_PROJECTM_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ProjectM
	REQUIRED_VARS PROJECTM_LIBRARY PROJECTM_INCLUDE_DIR
	VERSION_VAR PROJECTM_VERSION
)

if(PROJECTM_FOUND)
	set(PROJECTM_LIBRARIES ${PROJECTM_LIBRARY})
	set(PROJECTM_INCLUDE_DIRS ${PROJECTM_INCLUDE_DIR})

	if(NOT TARGET projectM::projectM)
		add_library(projectM::projectM UNKNOWN IMPORTED)
		set_target_properties(projectM::projectM PROPERTIES
			IMPORTED_LOCATION "${PROJECTM_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${PROJECTM_INCLUDE_DIR}"
		)
	endif()
endif()

mark_as_advanced(PROJECTM_INCLUDE_DIR PROJECTM_LIBRARY)
