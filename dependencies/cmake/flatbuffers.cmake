set(USE_SYSTEM_FLATBUFFERS_LIBS ${DEFAULT_USE_SYSTEM_FLATBUFFERS_LIBS} CACHE BOOL "use flatbuffers library from system")

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
		set_target_properties(flatc PROPERTIES IMPORTED_LOCATION ${FLATBUFFERS_FLATC_EXECUTABLE})
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

	# Disable Flatbuffers build tests
	set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "Build Flatbuffers with tests")

	# Disable Flatbuffers Compiler if cross compiling (and import or build nativly)
	include (CMakeDependentOption)
	CMAKE_DEPENDENT_OPTION(FLATBUFFERS_BUILD_FLATC "Enable the build of the flatbuffers compiler" OFF "CMAKE_CROSSCOMPILING" ON)

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
			set(FLATBUFFERS_COMPILER ${CMAKE_BINARY_DIR}/bin/flatc${CMAKE_EXECUTABLE_SUFFIX})

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
									-DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}
									-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
									-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
									-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
									-Wno-dev # We don't want to be warned over unused variables
				# BUILD_COMMAND       ${CMAKE_MAKE_PROGRAM} flatc
				BUILD_BYPRODUCTS    <BINARY_DIR>/Releease/flatc
			)

			add_executable(flatc IMPORTED GLOBAL)
			ExternalProject_Get_Property(flatc-host BINARY_DIR)
			set_target_properties(flatc PROPERTIES IMPORTED_LOCATION ${BINARY_DIR}/Releease/flatc)
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

	set_source_files_properties(${${FBS_GENERATED}} PROPERTIES GENERATED TRUE)
	set(${FBS_GENERATED} ${${FBS_GENERATED}} PARENT_SCOPE)
endfunction()
