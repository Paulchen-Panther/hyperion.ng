#  FindTurboJPEG.cmake
#  TURBOJPEG_FOUND
#  TurboJPEG_INCLUDE_DIR
#  TurboJPEG_LIBRARY

find_path(TurboJPEG_INCLUDE_DIR
	NAMES
		turbojpeg.h
	PATHS
		"C:/libjpeg-turbo64"
	PATH_SUFFIXES
		include
)

find_library(TurboJPEG_LIBRARY
	NAMES
		libturbojpeg
		turbojpeg
		turbojpeg-static
	PATHS
		"C:/libjpeg-turbo64"
	PATH_SUFFIXES
		bin
		lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TurboJPEG
	FOUND_VAR
		TurboJPEG_FOUND
	REQUIRED_VARS
		TurboJPEG_LIBRARY
		TurboJPEG_INCLUDE_DIR
)

if(TurboJPEG_FOUND)
	if(NOT TARGET turbojpeg)
		add_library(turbojpeg UNKNOWN IMPORTED GLOBAL)
		set_target_properties(turbojpeg PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${TurboJPEG_INCLUDE_DIR}"
			IMPORTED_LOCATION "${TurboJPEG_LIBRARY}"
		)
	endif()

	# libjpeg-turbo version detection from: https://github.com/chengfzy/CPlusPlusStudy/blob/master/cmake/FindTurboJpeg.cmake
	if(EXISTS "${TurboJPEG_INCLUDE_DIR}/jconfig.h")
		file(STRINGS "${TurboJPEG_INCLUDE_DIR}/jconfig.h" TurboJpegVersion REGEX "LIBJPEG_TURBO_VERSION ")
		string(REGEX REPLACE ".*VERSION *\(.*\).*" "\\1" TurboJPEG_VERSION "${TurboJpegVersion}")
	endif()

	if(TurboJPEG_VERSION)
		define_property(TARGET PROPERTY TURBOJPEG_VERSION_PROPERTY
			BRIEF_DOCS "TurboJPEG version property."
			FULL_DOCS "TurboJPEG version property."
		)

		set_target_properties(turbojpeg PROPERTIES
			TURBOJPEG_VERSION_PROPERTY ${TurboJPEG_VERSION}
		)
	endif()
endif()
