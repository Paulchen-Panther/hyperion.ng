find_path(JPEG_INCLUDE_DIR
	NAMES jpeglib.h
	PATHS "C:/libjpeg-turbo64"
	PATH_SUFFIXES include
)

find_library(JPEG_LIBRARY
	NAMES jpeg jpeg-static
	PATHS "C:/libjpeg-turbo64"
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JPEG
	FOUND_VAR JPEG_FOUND
	REQUIRED_VARS JPEG_LIBRARY JPEG_INCLUDE_DIR
	JPEG_INCLUDE_DIR JPEG_LIBRARY
)
