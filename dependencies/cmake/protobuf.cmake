set(USE_SYSTEM_PROTO_LIBS ${DEFAULT_USE_SYSTEM_PROTO_LIBS} CACHE BOOL "use protobuf library from system")

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

	# Disable Protobuf tests
	set(protobuf_BUILD_TESTS OFF CACHE BOOL "Build protobuf with tests")

	# Disable Protobuf without zlib
	set(protobuf_WITH_ZLIB OFF CACHE BOOL "Build protobuf with zlib support")

	# Build abeil (3rd party sub-module) with C++ version requirements
	set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "Build abseil-cpp with C++ version requirements propagated")

	# # Disable Protobuf Compiler if cross compiling (and import or build nativly)
	include (CMakeDependentOption)
	CMAKE_DEPENDENT_OPTION(protobuf_BUILD_PROTOC_BINARIES "Build protobuf libraries and protoc compiler" OFF "CMAKE_CROSSCOMPILING" ON)

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
			ExternalProject_Add(protoc-host
				PREFIX				${CMAKE_BINARY_DIR}/dependencies/external/protoc-host
				EXCLUDE_FROM_ALL    ON
				BUILD_ALWAYS		OFF
				DOWNLOAD_COMMAND	""
				INSTALL_COMMAND     ""
				SOURCE_DIR			${CMAKE_CURRENT_SOURCE_DIR}/external/protobuf
				CMAKE_ARGS          -Dprotobuf_BUILD_LIBPROTOC:BOOL=OFF
									-Dprotobuf_INSTALL:BOOL=OFF
									-Dprotobuf_BUILD_TESTS:BOOL=${protobuf_BUILD_TESTS}
									-Dprotobuf_BUILD_SHARED_LIBS:BOOL=${protobuf_BUILD_SHARED_LIBS}
									-Dprotobuf_WITH_ZLIB:BOOL=${protobuf_WITH_ZLIB}
									-DABSL_PROPAGATE_CXX_STD:BOOL=${ABSL_PROPAGATE_CXX_STD}
									-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}
									-DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}
									-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
									-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
									-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
									-Wno-dev # We don't want to be warned over unused variables
				BUILD_COMMAND       ${CMAKE_MAKE_PROGRAM} protoc
				BUILD_BYPRODUCTS    <BINARY_DIR>/protoc
				LOG_DOWNLOAD		OFF
				LOG_INSTALL			OFF
				LOG_CONFIGURE		OFF
				LOG_BUILD			OFF
			)

			add_executable(protoc IMPORTED GLOBAL)
			ExternalProject_Get_Property(protoc-host BINARY_DIR)
			set_target_properties(protoc PROPERTIES IMPORTED_LOCATION ${BINARY_DIR}/protoc)
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
		set_source_files_properties(${${SRCS}} ${${HDRS}}  ${ARGN} PROPERTIES COMPILE_FLAGS "-w -Wno-return-local-addr")
	elseif(MSVC)
		set_source_files_properties(${${SRCS}} ${${HDRS}}  ${ARGN} PROPERTIES COMPILE_FLAGS "/W0")
	endif()

	set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
	set(${SRCS} ${${SRCS}} PARENT_SCOPE)
	set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()
