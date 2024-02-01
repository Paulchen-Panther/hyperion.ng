#=============================================================================
# HIDAPI
#=============================================================================

if(ENABLE_DEV_USB_HID)
	# Find libusb
	find_package(libusb-1.0 QUIET REQUIRED)

	# Build HIDAPI as static library
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build statically version of HIDAPI")

	# Use the libusb based implementation of HIDAPI (Linux only)
	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		set(HIDAPI_WITH_HIDRAW OFF CACHE BOOL "Build HIDRAW-based implementation of HIDAPI")
	endif()

	# Suppress warnings about "Compatibility with CMake < 3.5 will be removed from a future version of CMake"
	set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

	# Install HIDAPI targets
	set(HIDAPI_INSTALL_TARGETS ON CACHE BOOL "Enable the installation of HIDAPI targets")

	# Disable build of HIDAPI tests
	set(HIDAPI_BUILD_HIDTEST OFF CACHE BOOL "Enable small console test application hidtest")

	# Add HIDAPI directory to the build
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/hidapi")
endif()

#=============================================================================
# Tinkerforge
#=============================================================================

if(ENABLE_DEV_TINKERFORGE)
	add_library(tinkerforge STATIC)
	target_sources(tinkerforge PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include/tinkerforge/bricklet_led_strip.h
		${CMAKE_CURRENT_SOURCE_DIR}/include/tinkerforge/ip_connection.h
		${CMAKE_CURRENT_SOURCE_DIR}/libsrc/tinkerforge/bricklet_led_strip.c
		${CMAKE_CURRENT_SOURCE_DIR}/libsrc/tinkerforge/ip_connection.c
	)

	target_include_directories(tinkerforge PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include/tinkerforge"
	)
endif()

#=============================================================================
# rpi_ws281x
#=============================================================================

if(ENABLE_DEV_WS281XPWM)
	# Build rpi_ws281x as static library
	set(BUILD_SHARED OFF CACHE BOOL "Build statically version of rpi_ws281x")

	# Disable build of rpi_ws281x test application
	set(BUILD_TEST OFF CACHE BOOL "Build test application")

	# Add rpi_ws281x directory to the build
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/rpi_ws281x")
endif()

#=============================================================================
# QMdnsEngine
#=============================================================================

if (ENABLE_MDNS)
	if(USE_SYSTEM_QMDNS_LIBS)
		find_package(qmdnsengine REQUIRED)
	else()
		if(CMAKE_CROSSCOMPILING AND CMAKE_OSX_ARCHITECTURES)
			string(REPLACE ";" "$<SEMICOLON>" CMAKE_OSX_ARCHITECTURES_ "${CMAKE_OSX_ARCHITECTURES}")
			set(CMAKE_MACOS_EXTRA_ARGS
				-DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES_}
				-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
			)
		endif()

		include(ExternalProject)
		ExternalProject_Add(qmdns
			PREFIX				${CMAKE_CURRENT_BINARY_DIR}/external/qmdnsengine
			BUILD_ALWAYS		OFF
			DOWNLOAD_COMMAND	""
			BINARY_DIR			${CMAKE_CURRENT_BINARY_DIR}/external/qmdnsengine/bin
			SOURCE_DIR			${CMAKE_SOURCE_DIR}/dependencies/external/qmdnsengine
			CMAKE_ARGS			-DBUILD_SHARED_LIBS:BOOL=OFF
								-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}
								-DBIN_INSTALL_DIR:STRING=lib
								-DLIB_INSTALL_DIR:STRING=lib
								-DINCLUDE_INSTALL_DIR:STRING=include
								-DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}
								-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
								-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
								-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
								-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
								-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
								-Wno-dev # We don't want to be warned over unused variables
								${CMAKE_MACOS_EXTRA_ARGS}
			INSTALL_DIR			${CMAKE_BINARY_DIR}
			BUILD_BYPRODUCTS    <INSTALL_DIR>/lib/${CMAKE_STATIC_LIBRARY_PREFIX}qmdnsengine${CMAKE_STATIC_LIBRARY_SUFFIX}
		)

		add_library(qmdnsengine STATIC IMPORTED GLOBAL)
		add_dependencies(qmdnsengine qmdns)
		ExternalProject_Get_Property(qmdns INSTALL_DIR)
		set_target_properties(qmdnsengine PROPERTIES
			IMPORTED_LOCATION "${INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}qmdnsengine${CMAKE_STATIC_LIBRARY_SUFFIX}"
			INTERFACE_INCLUDE_DIRECTORIES "${INSTALL_DIR}/include"
		)
	endif()
endif()

#=============================================================================
# FlatBuffers
#=============================================================================

if(ENABLE_FLATBUF_SERVER OR ENABLE_FLATBUF_CONNECT)
	if(USE_SYSTEM_FLATBUFFERS_LIBS)
		find_package(flatbuffers REQUIRED CONFIG NAMES flatbuffers Flatbuffers FlatBuffers)

		if(TARGET flatbuffers::flatbuffers AND NOT TARGET flatbuffers)
			add_library(flatbuffers INTERFACE IMPORTED GLOBAL)
			set_target_properties(flatbuffers PROPERTIES
				INTERFACE_LINK_LIBRARIES flatbuffers::flatbuffers
				INTERFACE_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:flatbuffers::flatbuffers,INTERFACE_INCLUDE_DIRECTORIES>
			)
		endif()

		if(TARGET flatbuffers::flatc AND NOT TARGET flatc)
			add_executable(flatc IMPORTED GLOBAL)
			get_target_property(FLATBUFFERS_FLATC_EXECUTABLE flatbuffers::flatc IMPORTED_LOCATION_RELEASE)
			set_target_properties(flatc PROPERTIES
				IMPORTED_LOCATION "${FLATBUFFERS_FLATC_EXECUTABLE}"
			)
		endif()

		if(NOT TARGET flatbuffers OR NOT TARGET flatc)
			# Fallback: build flatbuffers static libray inside project
			message(STATUS "Could not find Flatbuffers system library, build static Flatbuffers library")
			set(DEFAULT_USE_SYSTEM_FLATBUFFERS_LIBS OFF PARENT_SCOPE)
			set(USE_SYSTEM_FLATBUFFERS_LIBS OFF)
		endif()

		message(STATUS "Flatbuffers version used: ${flatbuffers_VERSION}")
	endif()

	if(NOT USE_SYSTEM_FLATBUFFERS_LIBS)
		# Build Flatbuffers as static library
		set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared Flatbuffers library")

		# Disable build of Flatbuffers tests
		set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "Build Flatbuffers with tests")

		# Disable Flatbuffers Compiler if cross compiling (and import or build nativly)
		include (CMakeDependentOption)
		cmake_dependent_option(FLATBUFFERS_BUILD_FLATC "Enable the build of the flatbuffers compiler" OFF "CMAKE_CROSSCOMPILING" ON)

		# Define the flatc import option
		set(IMPORT_FLATC "" CACHE STRING "Import the Flatbuffers compiler (flatc_export.cmake) from a native build")

		# Add Flatbuffers directory to the build
		add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/flatbuffers")

		if(CMAKE_CROSSCOMPILING)
			if(IMPORT_FLATC)
				# Import the Flatbuffers Compiler from a native build ...
				include(${IMPORT_FLATC})
			else()
				# ... or build flatc nativly
				set(FLATBUFFERS_COMPILER flatc${CMAKE_EXECUTABLE_SUFFIX})

				get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)


				include(ExternalProject)
				ExternalProject_Add(flatc-host
					PREFIX				${CMAKE_BINARY_DIR}/dependencies/external/flatc-host
					BUILD_ALWAYS		OFF
					DOWNLOAD_COMMAND	""
					INSTALL_COMMAND     ""
					SOURCE_DIR			${CMAKE_SOURCE_DIR}/dependencies/external/flatbuffers
					CMAKE_ARGS          -DFLATBUFFERS_BUILD_FLATLIB:BOOL=OFF
										-DFLATBUFFERS_INSTALL:BOOL=OFF
										-DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
										-DFLATBUFFERS_BUILD_TESTS:BOOL=${FLATBUFFERS_BUILD_TESTS}
										-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}
										-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
										-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
										-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
										-Wno-dev # We don't want to be warned over unused variables
					BUILD_BYPRODUCTS    <BINARY_DIR>/flatc
				)

				add_executable(flatc IMPORTED GLOBAL)
				ExternalProject_Get_Property(flatc-host BINARY_DIR)
				set_target_properties(flatc PROPERTIES
					IMPORTED_LOCATION "${BINARY_DIR}/flatc"
				)
				add_dependencies(flatc flatc-host)
			endif()
		else()
			# export the flatc compiler so it can be used when cross compiling
			export(TARGETS flatc FILE "${CMAKE_BINARY_DIR}/flatc_export.cmake")
		endif()

		get_build_interface_include_directory(TARGET flatbuffers OUTPUT FLATBUFFERS_INCLUDE_DIRS)
		if(FLATBUFFERS_INCLUDE_DIRS AND EXISTS "${FLATBUFFERS_INCLUDE_DIRS}/../package.json")
			file(STRINGS "${FLATBUFFERS_INCLUDE_DIRS}/../package.json" _FLATBUFFERS_VERSION_STRING REGEX "^[ \t\r\n]+\"version\":[ \t\r\n]+\"[0-9]+.[0-9]+.[0-9]+\",")
			string(REGEX REPLACE "^[ \t\r\n]+\"version\":[ \t\r\n]+\"([0-9]+.[0-9]+.[0-9]+)\"," "\\1" FLATBUFFERS_PARSE_VERSION "${_FLATBUFFERS_VERSION_STRING}")
			message(STATUS "Flatbuffers version used: ${FLATBUFFERS_PARSE_VERSION}")
		endif ()
	endif()

	function(compile_flatbuffer_schema FBS_GENERATED)
		if(NOT ARGN)
			message(SEND_ERROR "Error: compile_flatbuffer_schema() called without any schema files")
			return()
		endif()

		set(${FBS_GENERATED})
		foreach(FIL ${ARGN})
			get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
			get_filename_component(FIL_WE ${FIL} NAME_WE)

			set(OUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}_generated.h")
			list(APPEND ${FBS_GENERATED} ${OUT_FILE})

			add_custom_command(
				OUTPUT ${OUT_FILE}
				COMMAND $<TARGET_FILE:flatc>
				ARGS -c --no-includes --gen-mutable --gen-object-api -o "${CMAKE_CURRENT_BINARY_DIR}" "${ABS_FIL}"
				DEPENDS ${ABS_FIL} flatc
				COMMENT "Running flatbuffers compiler on ${FIL}"
				VERBATIM
			)
			set_property(SOURCE ${OUT_FILE} PROPERTY SKIP_AUTOMOC ON)
		endforeach()

		set_source_files_properties(${${FBS_GENERATED}} PROPERTIES
			GENERATED TRUE
		)
		set(${FBS_GENERATED} ${${FBS_GENERATED}} PARENT_SCOPE)
	endfunction()
endif()

#=============================================================================
# Protocol Buffers
#=============================================================================

if(ENABLE_PROTOBUF_SERVER)
	if(USE_SYSTEM_PROTO_LIBS)
		find_package(Protobuf REQUIRED)
		if(CMAKE_VERSION VERSION_GREATER 3.5.2)
			set(PROTOBUF_INCLUDE_DIRS ${Protobuf_INCLUDE_DIRS})
			set(PROTOBUF_PROTOC_EXECUTABLE ${Protobuf_PROTOC_EXECUTABLE})
			set(PROTOBUF_LIBRARIES ${Protobuf_LIBRARIES})
		endif()
	endif()

	if(NOT USE_SYSTEM_PROTO_LIBS)
		# Build Ptobuf as static library
		set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "Build protobuf shared")

		# Disable build of Protobuf tests
		set(protobuf_BUILD_TESTS OFF CACHE BOOL "Build protobuf with tests")

		# Disable build of Protobuf with zlib support
		set(protobuf_WITH_ZLIB OFF CACHE BOOL "Build protobuf with zlib support")

		# Build abeil (3rd party sub-module) with C++ version requirements
		set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "Build abseil-cpp with C++ version requirements propagated")

		# Disable Protobuf Compiler if cross compiling (and import or build nativly)
		include (CMakeDependentOption)
		cmake_dependent_option(protobuf_BUILD_PROTOC_BINARIES "Build protobuf libraries and protoc compiler" OFF "CMAKE_CROSSCOMPILING" ON)

		# Disable static linking of MSVC runtime libraries under Windows
		if (WIN32)
			set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "Build protobuf static")
		endif()

		# Define the protoc import option
		set(IMPORT_PROTOC "" CACHE STRING "Import the Protobuf compiler (protoc_export.cmake) from a native build")

		# Add Protobuf directory to the build
		add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/protobuf")

		if(CMAKE_CROSSCOMPILING)
			if(IMPORT_PROTOC)
				# Import the Protobuf Compiler from a native build ...
				include(${IMPORT_FLATC})
			else()
				# ... or build protoc nativly
				include(ExternalProject)
				get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
				ExternalProject_Add(protoc-host
					PREFIX				${CMAKE_BINARY_DIR}/dependencies/external/protoc-host
					BUILD_ALWAYS		OFF
					DOWNLOAD_COMMAND	""
					INSTALL_COMMAND     ""
					SOURCE_DIR			${CMAKE_SOURCE_DIR}/dependencies/external/protobuf
					CMAKE_ARGS          -Dprotobuf_BUILD_LIBPROTOC:BOOL=OFF
										-Dprotobuf_INSTALL:BOOL=OFF
										-Dprotobuf_BUILD_TESTS:BOOL=${protobuf_BUILD_TESTS}
										-Dprotobuf_BUILD_SHARED_LIBS:BOOL=${protobuf_BUILD_SHARED_LIBS}
										-Dprotobuf_WITH_ZLIB:BOOL=${protobuf_WITH_ZLIB}
										-DABSL_PROPAGATE_CXX_STD:BOOL=${ABSL_PROPAGATE_CXX_STD}
										-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}
										-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
										-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
										-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
										-Wno-dev # We don't want to be warned over unused variables
					BUILD_BYPRODUCTS    <BINARY_DIR>/protoc
				)

				add_executable(protoc IMPORTED GLOBAL)
				ExternalProject_Get_Property(protoc-host BINARY_DIR)
				set_target_properties(protoc PROPERTIES
					IMPORTED_LOCATION "${BINARY_DIR}/protoc"
				)
				add_dependencies(protoc protoc-host)
			endif()
		else()
			# export the protoc compiler so it can be used when cross compiling
			export(TARGETS protoc FILE "${CMAKE_BINARY_DIR}/protoc_export.cmake")
		endif()
	endif()

	function(protobuf_generate_cpp SRCS HDRS)
		if(NOT ARGN)
			message(SEND_ERROR "Error: PROTOBUF_GENERATE_CPP() called without any proto files")
			return()
		endif()

		if(PROTOBUF_GENERATE_CPP_APPEND_PATH)
			# Create an include path for each file specified
			foreach(FIL ${ARGN})
				get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
				get_filename_component(ABS_PATH ${ABS_FIL} PATH)
				list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
				if(${_contains_already} EQUAL -1)
					list(APPEND _protobuf_include_path -I ${ABS_PATH})
				endif()
			endforeach()
		else()
			set(_protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
		endif()

		if(DEFINED PROTOBUF_IMPORT_DIRS)
			foreach(DIR ${PROTOBUF_IMPORT_DIRS})
				get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
				list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
				if(${_contains_already} EQUAL -1)
					list(APPEND _protobuf_include_path -I ${ABS_PATH})
				endif()
			endforeach()
		endif()

		set(${SRCS})
		set(${HDRS})
		foreach(FIL ${ARGN})
			get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
			get_filename_component(FIL_WE ${FIL} NAME_WE)

			list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc")
			list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h")

			add_custom_command(
				OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc"
						"${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h"
				COMMAND $<TARGET_FILE:protoc>
				ARGS --cpp_out ${CMAKE_CURRENT_BINARY_DIR} ${_protobuf_include_path} ${ABS_FIL}
				DEPENDS ${ABS_FIL} protoc
				COMMENT "Running C++ protocol buffer compiler on ${FIL}"
				VERBATIM
			)
			set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc" PROPERTY SKIP_AUTOMOC ON)
			set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h" PROPERTY SKIP_AUTOMOC ON)
		endforeach()

		# disable warnings for auto generated proto files, we can't change the files ....
		if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
			set_source_files_properties(${${SRCS}} ${${HDRS}} ${ARGN} PROPERTIES
				COMPILE_FLAGS "-w -Wno-return-local-addr"
			)
		elseif(MSVC)
			set_source_files_properties(${${SRCS}} ${${HDRS}} ${ARGN} PROPERTIES
				COMPILE_FLAGS "/W0"
			)
		endif()

		set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES
			GENERATED TRUE
		)
		set(${SRCS} ${${SRCS}} PARENT_SCOPE)
		set(${HDRS} ${${HDRS}} PARENT_SCOPE)
	endfunction()
endif()

#=============================================================================
# mbedTLS
#=============================================================================

if(ENABLE_DEV_NETWORK)
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

		# Disable build of mbedTLS tests
		set(ENABLE_TESTING OFF CACHE BOOL "Build mbedTLS tests")

		# Disable fatal warnings
		option(MBEDTLS_FATAL_WARNINGS "Compiler warnings treated as errors" OFF)

		# Disable build of mbedTLS program building
		set(ENABLE_PROGRAMS OFF CACHE BOOL "Build mbedTLS programs")

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
		define_property(TARGET PROPERTY MBEDTLS_MAJOR_VERSION
			BRIEF_DOCS "Custom mbedTLS major version target property."
			FULL_DOCS "Custom mbedTLS major version target property."
		)
		set_target_properties(mbedtls PROPERTIES
			MBEDTLS_MAJOR_VERSION "${MAJOR_VERSION}"
		)
	endif()
endif(ENABLE_DEV_NETWORK)
