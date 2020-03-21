# FindTurboJPEG.cmake
# Uses environment variable TurboJPEG_ROOT as backup
# - TurboJPEG_FOUND
# - TurboJPEG_INCLUDE_DIRS
# - TurboJPEG_LIBRARY

FIND_PATH(TurboJPEG_INCLUDE_DIR
	NAMES turbojpeg.h
	PATHS
		"${DEPENDS_DIR}/libjpeg_turbo"
		"${DEPENDS_DIR}/libjpeg-turbo64"
		"/usr/local/opt/jpeg-turbo" # homebrew
		"/opt/local" # macports
		"/opt/libjpeg-turbo"
	ENV TurboJPEG_ROOT
	PATH_SUFFIXES include
)

FIND_LIBRARY(TurboJPEG_LIBRARY
	NAMES libturbojpeg.so.1 libturbojpeg.so.0 turbojpeg
	PATHS
		"${DEPENDS_DIR}/libjpeg_turbo"
		"${DEPENDS_DIR}/libjpeg-turbo64"
		"/usr/local/opt/jpeg-turbo" # homebrew
		"/opt/local" # macports
		"/opt/libjpeg-turbo"
	ENV TurboJPEG_ROOT
	PATH_SUFFIXES
		lib
		lib64
)

IF(TurboJPEG_INCLUDE_DIRS AND TurboJPEG_LIBRARY)
	INCLUDE(CheckCSourceCompiles)
	SET(CMAKE_REQUIRED_INCLUDES ${TurboJPEG_INCLUDE_DIRS})
	SET(CMAKE_REQUIRED_LIBRARIES ${TurboJPEG_LIBRARY})
	check_c_source_compiles("#include <turbojpeg.h>\nint main(void) { tjhandle h=tjInitCompress(); return 0; }" TURBOJPEG_WORKS)
	SET(CMAKE_REQUIRED_DEFINITIONS)
	SET(CMAKE_REQUIRED_INCLUDES)
	SET(CMAKE_REQUIRED_LIBRARIES)
ENDIF()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TurboJPEG FOUND_VAR TurboJPEG_FOUND
	REQUIRED_VARS TurboJPEG_LIBRARY TurboJPEG_INCLUDE_DIRS TURBOJPEG_WORKS)

