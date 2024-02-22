#=============================================================================
# Minimum required CMake version is 3.5.0
#=============================================================================

cmake_minimum_required(VERSION 3.5.0)

#=============================================================================
# Definitions that must happen before the project() command
#=============================================================================

if(CMAKE_HOST_APPLE)
	execute_process(COMMAND uname -m OUTPUT_VARIABLE HOST_SYSTEM_PROCESSOR OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(MACOS_ARCHITECTURE AND (NOT MACOS_ARCHITECTURE STREQUAL HOST_SYSTEM_PROCESSOR))
		set(CMAKE_SYSTEM_NAME "Darwin")
	endif()

	set(MACOS_ARCHITECTURE "${HOST_SYSTEM_PROCESSOR}" CACHE STRING "Target architecture for macOS. x86_64, arm64 or universal")

	if(MACOS_ARCHITECTURE STREQUAL "x86_64")
		set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "")
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
	elseif(MACOS_ARCHITECTURE STREQUAL "arm64")
		set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
		set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
	elseif(MACOS_ARCHITECTURE STREQUAL "universal")
		set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
		set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
	else()
		message(FATAL_ERROR "MACOS_ARCHITECTURE must be x86_64, arm64 or universal")
	endif()

	string(APPEND CMAKE_CXX_FLAGS "-stdlib=libc++")
endif()

#=============================================================================
# Our main project :)
#=============================================================================

project(hyperion)

#=============================================================================
# Set the required C++ standard
#=============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#=============================================================================
# Instruct CMake to run moc automatically when needed and auto prepare .qrc files
#=============================================================================

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

#=============================================================================
# Append our cmake module path (for include, find, etc.)
# Include the helper macros/functions
# Include CMakeDependentOption
#=============================================================================

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HelperMacros)
include(HelperFunctions)
include (CMakeDependentOption)

#=============================================================================
# Use buildcache (windows) or ccache (linux/macos) as compiler launcher
#=============================================================================

option(USE_COMPILER_CACHE "Enable compiler cache" OFF)
if(USE_COMPILER_CACHE)
	if(WIN32)
		set(LAUNCHER "buildcache")
	else()
		set(LAUNCHER "ccache")
	endif()

	find_program(CACHE_PROG ${LAUNCHER} REQUIRED)
	mark_as_advanced(CACHE_PROG)

	if(CACHE_PROG)
		message(STATUS "Building with ${LAUNCHER}: ${CACHE_PROG}")
		set(CMAKE_C_COMPILER_LAUNCHER "${CACHE_PROG}" CACHE STRING "C compiler launcher")
		set(CMAKE_CXX_COMPILER_LAUNCHER "${CACHE_PROG}" CACHE STRING "CXX compiler launcher")
	endif()
endif()

#=============================================================================
# Set output directories and add the project include directory
#=============================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	foreach(CONFIG_TYPE "RELEASE" "DEBUG" "RELWITHDEBINFO" "MINSIZEREL")
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
		set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
	endforeach()
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

#=============================================================================
# Parse semantic version of version file and write version to config
# Determine branch name and build id
#=============================================================================

file (STRINGS ".version" HYPERION_VERSION)
SetVersionNumber(HYPERION ${HYPERION_VERSION})
set(DEFAULT_JSON_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/hyperion.config.json.default)
file(READ ${DEFAULT_JSON_CONFIG_FILE} DEFAULT_JSON_CONFIG_VAR)
string(REPLACE "configVersionValue" ${HYPERION_VERSION} DEFAULT_JSON_CONFIG_VAR "${DEFAULT_JSON_CONFIG_VAR}")
string(REPLACE "previousVersionValue" ${HYPERION_VERSION} DEFAULT_JSON_CONFIG_VAR "${DEFAULT_JSON_CONFIG_VAR}")
file(WRITE ${CMAKE_BINARY_DIR}/config/hyperion.config.json.default "${DEFAULT_JSON_CONFIG_VAR}")

include(GitVersion)

#=============================================================================
# Determine/Evaluate the current platform and set auxiliary variables
#=============================================================================

if(NOT DEFINED PLATFORM)
	set(PLATFORM)
	DeterminePlatform(PLATFORM)
	if(NOT PLATFORM)
		message(FATAL_ERROR "PLATFORM is not defined and could not be evaluated. Set -DPLATFORM=<rpi|amlogic|amlogic-dev|amlogic64|amlogic64-dev|x11|x11-dev|osx>")
	endif()
else()
	message(STATUS "PLATFORM: ${PLATFORM}")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG_BUILD ON)
endif()

if(CMAKE_SYSTEM MATCHES "Linux")
	set(SYSTEM_MATCHES_LINUX ON)
endif()

if("${PLATFORM}" MATCHES "osx")
	set(PLATFORM_MATCHES_OSX ON)
elseif("${PLATFORM}" MATCHES "rpi")
	set(PLATFORM_MATCHES_RPI ON)
elseif("${PLATFORM}" MATCHES "^amlogic")
	set(PLATFORM_MATCHES_AML ON)
elseif("${PLATFORM}" MATCHES "^x11")
	set(PLATFORM_MATCHES_X11 ON)
elseif("${PLATFORM}" STREQUAL "imx6")
	set(PLATFORM_STREQUAL_IMX6 ON)
endif()

#=============================================================================
# Set the build options
#=============================================================================

# Build Hyperion with a reduced set of functionality, overwrites other options
option(HYPERION_LIGHT "Build Hyperion with a reduced set of functionality" OFF)

# Grabber options
cmake_dependent_option(ENABLE_AMLOGIC			"Enable the AMLOGIC video grabber"			ON "NOT HYPERION_LIGHT;PLATFORM_MATCHES_AML"												OFF)
cmake_dependent_option(ENABLE_DISPMANX			"Enable the RPi dispmanx grabber"			ON "NOT HYPERION_LIGHT;PLATFORM_MATCHES_RPI"												OFF)
cmake_dependent_option(ENABLE_DX				"Enable the DirectX grabber"				ON "NOT HYPERION_LIGHT;WIN32"																OFF)
cmake_dependent_option(ENABLE_FB				"Enable the framebuffer grabber"			ON "NOT HYPERION_LIGHT;ENABLE_AMLOGIC OR SYSTEM_MATCHES_LINUX OR PLATFORM_STREQUAL_IMX6"	OFF)
cmake_dependent_option(ENABLE_MF				"Enable the Media Foundation grabber"		ON "NOT HYPERION_LIGHT;WIN32"																OFF)
cmake_dependent_option(ENABLE_OSX				"Enable the OSX grabber"					ON "NOT HYPERION_LIGHT;PLATFORM_MATCHES_OSX"												OFF)
cmake_dependent_option(ENABLE_QT				"Enable the Qt grabber"						ON "NOT HYPERION_LIGHT"																		OFF)
cmake_dependent_option(ENABLE_V4L2				"Enable the V4L2 grabber"					ON "NOT HYPERION_LIGHT;SYSTEM_MATCHES_LINUX"												OFF)
cmake_dependent_option(ENABLE_X11				"Enable the X11 grabber"					ON "NOT HYPERION_LIGHT;PLATFORM_MATCHES_X11"												OFF)
cmake_dependent_option(ENABLE_XCB				"Enable the XCB grabber"					ON "NOT HYPERION_LIGHT;PLATFORM_MATCHES_X11"												OFF)
cmake_dependent_option(ENABLE_AUDIO				"Enable the AUDIO grabber"					ON "NOT HYPERION_LIGHT;NOT PLATFORM_MATCHES_OSX"											OFF)

# Input options
cmake_dependent_option(ENABLE_BOBLIGHT_SERVER	"Enable BOBLIGHT server"					ON "NOT HYPERION_LIGHT"																		OFF)
cmake_dependent_option(ENABLE_CEC				"Enable the libcec and CEC control"			ON "NOT HYPERION_LIGHT;SYSTEM_MATCHES_LINUX"												OFF)
cmake_dependent_option(ENABLE_FLATBUF_SERVER	"Enable Flatbuffers server"					ON "NOT HYPERION_LIGHT"																		OFF)
cmake_dependent_option(ENABLE_PROTOBUF_SERVER	"Enable Protocol Buffers server"			ON "NOT HYPERION_LIGHT"																		OFF)

# Output options
cmake_dependent_option(ENABLE_FORWARDER			"Enable Hyperion forwarding"				ON "NOT HYPERION_LIGHT"																		OFF)
cmake_dependent_option(ENABLE_FLATBUF_CONNECT	"Enable Flatbuffers connecting remotely"	ON "NOT HYPERION_LIGHT OR ENABLE_FORWARDER"													OFF)

# LED-Device options
cmake_dependent_option(ENABLE_DEV_NETWORK		"Enable the Network devices"				ON ""																						OFF)
cmake_dependent_option(ENABLE_DEV_SERIAL		"Enable the Serial devices"					ON ""																						OFF)
cmake_dependent_option(ENABLE_DEV_SPI			"Enable the SPI device"						ON "SYSTEM_MATCHES_LINUX"																	OFF)
cmake_dependent_option(ENABLE_DEV_TINKERFORGE	"Enable the TINKERFORGE device"				ON "SYSTEM_MATCHES_LINUX"																	OFF)
cmake_dependent_option(ENABLE_DEV_USB_HID		"Enable the libusb and hid devices"			ON ""																						OFF)
cmake_dependent_option(ENABLE_DEV_WS281XPWM		"Enable the WS281x-PWM device"				ON "PLATFORM_MATCHES_RPI OR PLATFORM_MATCHES_X11"											OFF)

# Services options
cmake_dependent_option(ENABLE_EFFECTENGINE		"Enable Effect-Engine"						ON "NOT HYPERION_LIGHT"																		OFF)
cmake_dependent_option(ENABLE_MDNS				"Enable mDNS (aka Zeroconf)"				ON ""																						OFF)

# Standalone binaries
cmake_dependent_option(ENABLE_REMOTE_CTL		"Enable standalone Hyperion remote control"	ON "NOT PLATFORM_MATCHES_OSX"																OFF)
cmake_dependent_option(ENABLE_AMLOGIC_EXT		"Enable standalone AMLOGIC video grabber"	ON "ENABLE_AMLOGIC;ENABLE_FLATBUF_CONNECT"													OFF)
cmake_dependent_option(ENABLE_V4L2_EXT			"Enable standalone V4L2 grabber"			ON "ENABLE_V4L2;ENABLE_FLATBUF_CONNECT"														OFF)
cmake_dependent_option(ENABLE_X11_EXT			"Enable standalone X11 grabber"				ON "ENABLE_X11;ENABLE_FLATBUF_CONNECT"														OFF)
cmake_dependent_option(ENABLE_XCB_EXT			"Enable standalone XCB grabber"				ON "ENABLE_XCB;ENABLE_FLATBUF_CONNECT"														OFF)
cmake_dependent_option(ENABLE_DISPMANX_EXT		"Enable standalone RPi dispmanx grabber"	ON "ENABLE_DISPMANX;ENABLE_FLATBUF_CONNECT"													OFF)
cmake_dependent_option(ENABLE_FB_EXT			"Enable standalone framebuffer grabber"		ON "ENABLE_FB;ENABLE_FLATBUF_CONNECT"														OFF)
cmake_dependent_option(ENABLE_QT_EXT			"Enable standalone Qt grabber"				ON "ENABLE_QT;ENABLE_FLATBUF_CONNECT"														OFF)
cmake_dependent_option(ENABLE_OSX_EXT			"Enable standalone OSX grabber"				ON "ENABLE_OSX;ENABLE_FLATBUF_CONNECT"														OFF)

# 3rd party
option(USE_SYSTEM_LIBUSB_LIBS					"Use LibUSB library from system"																										ON)
option(USE_SYSTEM_QMDNS_LIBS					"Use QMdnsEngine library from system"																									ON)
option(USE_SYSTEM_FLATBUFFERS_LIBS				"Use FlatBuffers library from system"																									ON)
option(USE_SYSTEM_PROTO_LIBS					"Use Protocol Buffers library from system"																								ON)
option(USE_SYSTEM_MBEDTLS_LIBS					"Use mbedTLS library from system"																										ON)

# Tests
cmake_dependent_option(ENABLE_TESTS				"Compile additional test applications"		ON "DEBUG_BUILD"																			OFF)

# Advanced features
cmake_dependent_option(ENABLE_DEPLOY_DEPENDENCIES "Deploy with dependencies"				ON "NOT PLATFORM_MATCHES_OSX;NOT WIN32;SYSTEM_MATCHES_LINUX"								OFF)
option(ENABLE_JSONCHECKS						"Validate json schema files"																											ON )
option(ENABLE_EXPERIMENTAL						"Compile experimental features"																											OFF)
option(ENABLE_PROFILER							"enable profiler capabilities - not for release code"																					OFF)

#=============================================================================
# Configure the predefined header file to pass selected options to the code
#=============================================================================

configure_file("${PROJECT_SOURCE_DIR}/HyperionConfig.h.in" "${PROJECT_BINARY_DIR}/HyperionConfig.h")
include_directories(${PROJECT_BINARY_DIR})

# Setup header file cleanup job
if(NOT CMAKE_VERSION VERSION_LESS "3.15")
	set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_BINARY_DIR}/HyperionConfig.h)
else()
	set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_BINARY_DIR}/HyperionConfig.h)
endif()

#=============================================================================
# Platform-specific tasks
#=============================================================================

# Search for Windows SDK
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	find_package(WindowsSDK REQUIRED)
endif()

# If Hyperion is deployed with all dependencies under Linux
# it is necessary to deactivate the linker option dtag
if(ENABLE_DEPLOY_DEPENDENCIES AND SYSTEM_MATCHES_LINUX)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--disable-new-dtags")
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--disable-new-dtags")
	set(CMAKE_SKIP_BUILD_RPATH OFF)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
	set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
endif()

#=============================================================================
# Allow to overwrite QT base directory
# Either supply QTDIR as -DQTDIR=<path> to cmake or set and environment variable QTDIR pointing to the Qt installation
# For Windows and OSX, the default Qt installation path are tried to resolved automatically
#=============================================================================

if(NOT DEFINED QTDIR AND NOT VCPKG_TOOLCHAIN)
	if(DEFINED ENV{QTDIR})
		set(QTDIR $ENV{QTDIR})
	else()
		if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
			FIRSTSUBDIR(SUBDIRQT "C:/Qt")
			if(NOT ${SUBDIRQT} STREQUAL "")
				set(QTDIR "${SUBDIRQT}/msvc2019_64")
			endif()
		elseif(PLATFORM_MATCHES_OSX)
			foreach(qt_version 5 6)

			endforeach()

			# QT6 x86_64 location
			if(EXISTS /usr/local/opt/qt6)
				set(QTDIR "/usr/local/opt/qt6")
			elseif(EXISTS /usr/local/opt/qt@6)
				set(QTDIR "/usr/local/opt/qt@6")
			# QT6 arm64 location
			elseif(EXISTS /opt/homebrew/opt/qt6)
				set(QTDIR "/opt/homebrew/opt/qt6")
			elseif(EXISTS /opt/homebrew/opt/qt@6)
				set(QTDIR "/opt/homebrew/opt/qt@6")
			# QT5 x86_64 location
			elseif(EXISTS /usr/local/opt/qt5)
				set(QTDIR "/usr/local/opt/qt5")
			elseif(EXISTS /usr/local/opt/qt@5)
				set(QTDIR "/usr/local/opt/qt@5")
			# QT5 arm64 location
			elseif(EXISTS /opt/homebrew/opt/qt5)
				set(QTDIR "/opt/homebrew/opt/qt5")
			elseif(EXISTS /opt/homebrew/opt/qt@5)
				set(QTDIR "/opt/homebrew/opt/qt@5")
			endif()
		endif()
	endif()
endif()

if(DEFINED QTDIR)
	message(STATUS "Added QTDIR '${QTDIR}' to CMAKE_PREFIX_PATH")
	list(PREPEND CMAKE_PREFIX_PATH
		${QTDIR}
		${QTDIR}/lib
	)
endif()

if(CMAKE_PREFIX_PATH)
	message(STATUS "CMAKE_PREFIX_PATH used: ${CMAKE_PREFIX_PATH}")
endif()

# find QT libs
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui Network Sql Widgets REQUIRED)
message(STATUS "Found Qt Version: ${QT_VERSION}")

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
	set(QT_MIN_VERSION "6.2.2")
else()
	set(QT_MIN_VERSION "5.5.0")
endif()

if("${QT_VERSION}" VERSION_LESS "${QT_MIN_VERSION}")
	message(FATAL_ERROR "Your Qt version is to old! Minimum required ${QT_MIN_VERSION}")
endif()

find_package(Qt${QT_VERSION_MAJOR} ${QT_VERSION} COMPONENTS Core Gui Network Sql Widgets REQUIRED)

#=============================================================================
# Check JSON files/schemas if the JSON check option 'ENABLE_JSONCHECKS' ON
#=============================================================================

if(ENABLE_JSONCHECKS)
	if(${CMAKE_VERSION} VERSION_LESS "3.12.0")
		find_package(PythonInterp 3.5 QUIET REQUIRED)
	else()
		find_package(Python3 COMPONENTS Interpreter Development QUIET REQUIRED)
		set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
	endif()

	# check all json files
	file(GLOB_RECURSE HYPERION_SCHEMAS RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libsrc/*schema*.json)
	set(JSON_FILES ${CMAKE_BINARY_DIR}/config/hyperion.config.json.default ${HYPERION_SCHEMAS})

	execute_process(
		COMMAND ${PYTHON_EXECUTABLE} test/jsonchecks/checkjson.py ${JSON_FILES}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE CHECK_JSON_FAILED
	)

	if(${CHECK_JSON_FAILED})
		message(FATAL_ERROR "Check of JSON files failed")
	endif()

	execute_process(
		COMMAND ${PYTHON_EXECUTABLE} test/jsonchecks/checkschema.py ${CMAKE_BINARY_DIR}/config/hyperion.config.json.default libsrc/hyperion/hyperion.schema.json
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE CHECK_CONFIG_FAILED
	)

	if(${CHECK_CONFIG_FAILED})
		message(FATAL_ERROR "Check of JSON default config failed")
	endif()
endif()

#=============================================================================
# Add our Code
#=============================================================================

add_subdirectory(dependencies)
add_subdirectory(resources)
add_subdirectory(libsrc)
add_subdirectory(src)
if(ENABLE_TESTS)
	add_subdirectory(test)
endif()

#=============================================================================
# Create uninstall target
#=============================================================================

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)

#=============================================================================
# Add Packaging and print configuration summary
#=============================================================================

include(Packaging)
include(Summary)
