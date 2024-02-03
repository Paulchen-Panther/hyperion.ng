find_package(Qt${QT_VERSION_MAJOR}Core REQUIRED)

# Find the windeployqt binaries
get_target_property(QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE
	NAMES
		windeployqt
	HINTS
		${QT_BIN_DIR}
)

# Collect the runtime libraries
if (QT_VERSION_MAJOR EQUAL 5)
	set(WINDEPLOYQT_PARAMS --dry-run --no-angle --no-opengl-sw --list mapping)
else()
	set(WINDEPLOYQT_PARAMS --dry-run --no-opengl-sw --list mapping)
endif()

get_filename_component(COMPILER_PATH "${CMAKE_CXX_COMPILER}" DIRECTORY)
execute_process(
	COMMAND ${CMAKE_COMMAND} -E env "PATH=${COMPILER_PATH};${QT_BIN_DIR}"
	${WINDEPLOYQT_EXECUTABLE} ${WINDEPLOYQT_PARAMS} ${TARGET_FILE}
	OUTPUT_VARIABLE DEPS
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Parse DEPS into a semicolon-separated list.
separate_arguments(DEPENDENCIES WINDOWS_COMMAND ${DEPS})
string(REPLACE "\\" "/" DEPENDENCIES "${DEPENDENCIES}")

# Copy dependencies to 'hyperion/lib' or 'hyperion'
while(DEPENDENCIES)
	list(GET DEPENDENCIES 0 src)
	list(GET DEPENDENCIES 1 dst)
	get_filename_component(dst ${dst} DIRECTORY)

	if(NOT ${dst} STREQUAL "")
		set(DESTINATION_DIR "lib/${dst}")
	else()
		set(DESTINATION_DIR "bin")
	endif()

	install(
		FILES
			${src}
		DESTINATION
			${DESTINATION_DIR}
		COMPONENT
			"Hyperion"
	)

	list(REMOVE_AT DEPENDENCIES 0 1)
endwhile()

# Copy libssl/libcrypto to 'hyperion'
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
	string(REGEX MATCHALL "[0-9]+" openssl_versions "${OPENSSL_VERSION}")
	list(GET openssl_versions 0 openssl_version_major)
	list(GET openssl_versions 1 openssl_version_minor)

	set(open_ssl_version_suffix)
	if (openssl_version_major VERSION_EQUAL 1 AND openssl_version_minor VERSION_EQUAL 1)
		set(open_ssl_version_suffix "-1_1")
	else()
		set(open_ssl_version_suffix "-3")
	endif()

	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		string(APPEND open_ssl_version_suffix "-x64")
	endif()

	get_filename_component(OPENSSL_DIR ${OPENSSL_INCLUDE_DIR} DIRECTORY)
	find_file(OPENSSL_DLLs
		NAMES
			"libssl${open_ssl_version_suffix}.dll"
			"libcrypto${open_ssl_version_suffix}.dll"
		NO_DEFAULT_PATH
		HINTS
			${OPENSSL_DIR}
			${OPENSSL_DIR}/bin
	)

	install(
		FILES
			${OPENSSL_DLLs}
		DESTINATION
			"bin"
		COMPONENT
			"Hyperion"
	)
endif()

# Copy libjpeg-turbo to 'hyperion'
if(ENABLE_MF)
	find_package(TurboJPEG)
	if(TurboJPEG_FOUND AND (TARGET turbojpeg))
		get_target_property(TurboJPEG_INCLUDE_DIR turbojpeg INTERFACE_INCLUDE_DIRECTORIES)
		get_filename_component(TurboJPEG_BASE_DIR ${TurboJPEG_INCLUDE_DIR} DIRECTORY)
		message(STATUS "${TURBOJPEG_LOCATION}")
		find_file(TURBOJPEG_DLLs
			NAMES
				"turbojpeg.dll"
				"jpeg62.dll"
			NO_DEFAULT_PATH
			HINTS
				${TurboJPEG_BASE_DIR}
				${TurboJPEG_BASE_DIR}/bin
		)

		install(
			FILES
				${TURBOJPEG_DLLs}
			DESTINATION
				"bin"
			COMPONENT
				"Hyperion"
		)
	endif()
endif()

# Create a qt.conf file in 'bin' to override hard-coded search paths in Qt plugins
file(WRITE "${CMAKE_BINARY_DIR}/qt.conf" "[Paths]\nPlugins=../lib/\n")
install(
	FILES
		"${CMAKE_BINARY_DIR}/qt.conf"
	DESTINATION
		"bin"
	COMPONENT
		"Hyperion"
)

if(ENABLE_EFFECTENGINE)
	# Download embedable python package (only release build package available)
	# Currently only cmake version >= 3.12 implemented
	set(url "https://www.python.org/ftp/python/${Python3_VERSION}/")
	set(filename "python-${Python3_VERSION}-embed-amd64.zip")

	if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${filename}" OR NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/python")
		file(DOWNLOAD "${url}${filename}" "${CMAKE_CURRENT_BINARY_DIR}/${filename}"
			STATUS result
		)

		# Check if the download is successful
		list(GET result 0 result_code)
		if(NOT result_code EQUAL 0)
			list(GET result 1 reason)
			message(FATAL_ERROR "Could not download file ${url}${filename}: ${reason}")
		endif()

		# Unpack downloaded embed python
		file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/python)
		file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/python)
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar -xfz "${CMAKE_CURRENT_BINARY_DIR}/${filename}"
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/python
			OUTPUT_QUIET
		)
	endif()

	# Copy pythonXX.dll and pythonXX.zip to 'hyperion'
	foreach(PYTHON_FILE
		"python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll"
		"python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.zip"
	)
		install(
			FILES
				${CMAKE_CURRENT_BINARY_DIR}/python/${PYTHON_FILE}
			DESTINATION
				"bin"
			COMPONENT
				"Hyperion"
		)
	endforeach()
endif()

if(ENABLE_DX)
	# Download DirectX End-User Runtimes (June 2010)
	set(url "https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe")
	if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/dx_redist.exe")
		file(DOWNLOAD "${url}" "${CMAKE_CURRENT_BINARY_DIR}/dx_redist.exe"
			STATUS result
		)

		# Check if the download is successful
		list(GET result 0 result_code)
		if(NOT result_code EQUAL 0)
			list(GET result 1 reason)
			message(FATAL_ERROR "Could not download DirectX End-User Runtimes: ${reason}")
		endif()
	endif()

	# Copy DirectX End-User Runtimes to 'hyperion'
	install(
		FILES
			${CMAKE_CURRENT_BINARY_DIR}/dx_redist.exe
		DESTINATION
			"bin"
		COMPONENT
			"Hyperion"
	)
endif()
