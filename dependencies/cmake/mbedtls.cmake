set(USE_SYSTEM_MBEDTLS_LIBS ${DEFAULT_USE_SYSTEM_MBEDTLS_LIBS} CACHE BOOL "use mbedtls library from system")

if(USE_SYSTEM_MBEDTLS_LIBS)
	find_package(mbedtls REQUIRED)
	if(NOT MBEDTLS_FOUND)
		message(STATUS "Could NOT find mbedTLS system libraries, build static mbedTLS libraries")
		# Fallback: build mbedTLS static libray inside project
		set(DEFAULT_USE_SYSTEM_MBEDTLS_LIBS OFF PARENT_SCOPE)
		set(USE_SYSTEM_MBEDTLS_LIBS OFF)
	else()
		set(BUILD_OR_SYSTEM "system")
	endif()
endif()

if(NOT USE_SYSTEM_MBEDTLS_LIBS)
	# Build mbedTLS as static library
	set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "Disable mbedTLS shared libraries")
	set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "Enable mbedTLS static libraries")
	set(BUILD_OR_SYSTEM "static")

	# Disable mbedTLS tests
	set(ENABLE_TESTING OFF CACHE BOOL "Disable mbedTLS tests")

	# Disable fatal warnings
	option(MBEDTLS_FATAL_WARNINGS "Compiler warnings treated as errors" OFF)

	# Disable mbedTLS program building
	set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable mbedTLS programs")

	# Add mbedTLS directory to the build
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/mbedtls")
endif()

# Get current target include directory and get mbedtls version
get_build_interface_include_directory(TARGET mbedtls OUTPUT MBEDTLS_INCLUDE_DIR)
if(EXISTS "${MBEDTLS_INCLUDE_DIR}/mbedtls/build_info.h")
	file(STRINGS ${MBEDTLS_INCLUDE_DIR}/mbedtls/build_info.h _MBEDTLS_VERSION_LINE REGEX "^#define[ \t]+MBEDTLS_VERSION_STRING[\t ].*")
	string(REGEX REPLACE ".*MBEDTLS_VERSION_STRING[\t ]+\"(.*)\"" "\\1" MBEDTLS_VERSION ${_MBEDTLS_VERSION_LINE})
elseif(EXISTS "${MBEDTLS_INCLUDE_DIR}/mbedtls/version.h")
	file(STRINGS "${MBEDTLS_INCLUDE_DIR}/mbedtls/version.h" _MBEDTLS_VERSION_STRING REGEX "^#[\t ]*define[\t ]+MBEDTLS_VERSION_STRING[\t ]+\"[0-9]+.[0-9]+.[0-9]+\"")
	string(REGEX REPLACE "^.*MBEDTLS_VERSION_STRING.*([0-9]+.[0-9]+.[0-9]+).*" "\\1" MBEDTLS_VERSION "${_MBEDTLS_VERSION_STRING}")
endif()

# Define and set custom MBEDTLS_MAJOR_VERSION target property
if(TARGET mbedtls AND MBEDTLS_VERSION)
	message(STATUS "Using ${BUILD_OR_SYSTEM} mbedtls library (build version \"${MBEDTLS_VERSION}\")")
	string(REGEX MATCH "[0-9]+|-([A-Za-z0-9_.]+)" MAJOR_VERSION ${MBEDTLS_VERSION})
	define_property(TARGET PROPERTY MBEDTLS_MAJOR_VERSION)
	set_target_properties(mbedtls PROPERTIES MBEDTLS_MAJOR_VERSION ${MAJOR_VERSION})
endif()
