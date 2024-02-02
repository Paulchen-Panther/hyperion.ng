#  FindTurboJPEG.cmake
#  TURBOJPEG_FOUND
#  TurboJPEG_INCLUDE_DIR
#  TurboJPEG_LIBRARY

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(PC_TurboJPEG QUIET libturbojpeg)
	if(DEFINED PC_TurboJPEG_VERSION AND NOT PC_TurboJPEG_VERSION STREQUAL "")
		set(TurboJPEG_VERSION "${PC_TurboJPEG_VERSION}")
	endif()
endif()

find_path(TurboJPEG_INCLUDE_DIR
	NAMES
		turbojpeg.h
	PATHS
		"C:/libjpeg-turbo64"
	HINTS
		${PC_TurboJPEG_INCLUDEDIR}
		${PC_TurboJPEG_INCLUDE_DIRS}
	PATH_SUFFIXES
		include
)

find_library(TurboJPEG_LIBRARY
	NAMES
		${PC_TurboJPEG_LIBRARIES}
		turbojpeg
		turbojpeg-static
	PATHS
		"C:/libjpeg-turbo64"
	HINTS
		${PC_TurboJPEG_LIBDIR}
		${PC_TurboJPEG_LIBRARY_DIRS}
	PATH_SUFFIXES
		bin
		lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TurboJPEG
	REQUIRED_VARS TurboJPEG_LIBRARY TurboJPEG_INCLUDE_DIR
	HANDLE_COMPONENTS
	TurboJPEG_INCLUDE_DIR TurboJPEG_LIBRARY
)


if(TurboJPEG_FOUND)
	add_library(turbojpeg UNKNOWN IMPORTED GLOBAL)
	set_target_properties(turbojpeg PROPERTIES
		IMPORTED_LOCATION "${TurboJPEG_LIBRARY}"
		INTERFACE_COMPILE_OPTIONS "${PC_TurboJPEG_CFLAGS} ${PC_TurboJPEG_CFLAGS_OTHER}"
		INTERFACE_INCLUDE_DIRECTORIES "${TurboJPEG_INCLUDE_DIR}"
		INTERFACE_LINK_LIBRARIES "${PC_TurboJPEG_LINK_LIBRARIES}"
	)

	if(TurboJPEG_VERSION)
		define_property(TARGET PROPERTY TURBOJPEG_VERSION_PROPERTY
			BRIEF_DOCS "TurboJPEG version property."
			FULL_DOCS "TurboJPEG version property."
		)

		set_target_properties(turbojpeg PROPERTIES
		TURBOJPEG_VERSION_PROPERTY "${TurboJPEG_VERSION}"
		)
	endif()
endif()
