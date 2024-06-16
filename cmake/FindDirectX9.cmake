# Find the DirectX 9 includes and library
# This module defines:
#  DIRECTX9_INCLUDE_DIRS, where to find d3d9.h, etc.
#  DIRECTX9_LIBRARIES, libraries to link against to use DirectX.
#  DIRECTX9_FOUND, If false, do not try to use DirectX.

set(DIRECTX9_PATHS
	"$ENV{DXSDK_DIR}"
	"$ENV{DIRECTX_ROOT}"
	"C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)"
	"C:/Program Files/Microsoft DirectX SDK (June 2010)"
)

find_path(DIRECTX9_INCLUDE_DIRS
	NAMES
		d3dx9.h
	PATHS
		${DIRECTX9_PATHS}
	PATH_SUFFIXES
		include
		include/directxsdk
)

foreach(DXLIB "d3d9" "d3dx9" "DxErr")
	find_library(DIRECTX9_${DXLIB}_LIBRARY
		NAMES
			${DXLIB}
		PATHS
			${DIRECTX9_PATHS}
		PATH_SUFFIXES
			Lib/x64
			Lib
	)
endforeach()

set(DIRECTX9_LIBRARIES ${DIRECTX9_d3d9_LIBRARY} ${DIRECTX9_d3dx9_LIBRARY} ${DIRECTX9_DxErr_LIBRARY})

# handle the QUIETLY and REQUIRED arguments and set DIRECTX9_FOUND to TRUE if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DirectX9 DEFAULT_MSG DIRECTX9_LIBRARIES DIRECTX9_INCLUDE_DIRS)
mark_as_advanced(DIRECTX9_INCLUDE_DIRS DIRECTX9_d3d9_LIBRARY DIRECTX9_d3dx9_LIBRARY DIRECTX9_DxErr_LIBRARY)
