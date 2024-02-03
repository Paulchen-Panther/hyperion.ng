find_package(Qt${QT_VERSION_MAJOR}Core REQUIRED)
find_package(OpenSSL REQUIRED)

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
get_filename_component(COMPILER_PATH "${CMAKE_CXX_COMPILER}" DIRECTORY)
if (QT_VERSION_MAJOR EQUAL 5)
    set(WINDEPLOYQT_PARAMS --no-angle --no-opengl-sw)
else()
    set(WINDEPLOYQT_PARAMS --no-opengl-sw)
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E
    env "PATH=${COMPILER_PATH};${QT_BIN_DIR}" "${WINDEPLOYQT_EXECUTABLE}"
    --dry-run
    ${WINDEPLOYQT_PARAMS}
    --list mapping
    "${TARGET_FILE}"
    OUTPUT_VARIABLE DEPS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Parse DEPS into a semicolon-separated list.
separate_arguments(DEPENDENCIES WINDOWS_COMMAND ${DEPS})
string(REPLACE "\\" "/" DEPENDENCIES "${DEPENDENCIES}")

# Copy dependencies to 'hyperion/lib' or 'hyperion'
while (DEPENDENCIES)
    list(GET DEPENDENCIES 0 src)
    list(GET DEPENDENCIES 1 dst)
    get_filename_component(dst ${dst} DIRECTORY)

    if (NOT "${dst}" STREQUAL "")
        install(
            FILES ${src}
            DESTINATION "lib/${dst}"
            COMPONENT "Hyperion"
        )
    else()
        install(
            FILES ${src}
            DESTINATION "bin"
            COMPONENT "Hyperion"
        )
    endif()

    list(REMOVE_AT DEPENDENCIES 0 1)
endwhile()

# Copy libssl/libcrypto to 'hyperion'
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

    find_file(OPENSSL_SSL
        NAMES "libssl${open_ssl_version_suffix}.dll"
        PATHS ${OPENSSL_INCLUDE_DIR}/.. ${OPENSSL_INCLUDE_DIR}/../bin
        NO_DEFAULT_PATH
    )

    find_file(OPENSSL_CRYPTO
        NAMES "libcrypto${open_ssl_version_suffix}.dll"
        PATHS ${OPENSSL_INCLUDE_DIR}/.. ${OPENSSL_INCLUDE_DIR}/../bin
        NO_DEFAULT_PATH
    )

    install(
        FILES ${OPENSSL_SSL} ${OPENSSL_CRYPTO}
        DESTINATION "bin"
        COMPONENT "Hyperion"
    )
endif(OPENSSL_FOUND)

# Copy libjpeg-turbo to 'hyperion'
if (ENABLE_MF)
    find_package(TurboJPEG)

    if (TurboJPEG_FOUND)
        find_file(TURBOJPEG_DLL
            NAMES "turbojpeg.dll"
            PATHS ${TurboJPEG_INCLUDE_DIRS}/.. ${TurboJPEG_INCLUDE_DIRS}/../bin
            NO_DEFAULT_PATH
        )

        find_file(JPEG_DLL
            NAMES "jpeg62.dll"
            PATHS ${TurboJPEG_INCLUDE_DIRS}/.. ${TurboJPEG_INCLUDE_DIRS}/../bin
            NO_DEFAULT_PATH
        )

        install(
            FILES ${TURBOJPEG_DLL} ${JPEG_DLL}
            DESTINATION "bin"
            COMPONENT "Hyperion"
        )
    endif()
endif(ENABLE_MF)

# Create a qt.conf file in 'bin' to override hard-coded search paths in Qt plugins
file(WRITE "${CMAKE_BINARY_DIR}/qt.conf" "[Paths]\nPlugins=../lib/\n")
install(
    FILES "${CMAKE_BINARY_DIR}/qt.conf"
    DESTINATION "bin"
    COMPONENT "Hyperion"
)

if(ENABLE_EFFECTENGINE)
    # Download embed python package (only release build package available)
    # Currently only cmake version >= 3.12 implemented
    set(url "https://www.python.org/ftp/python/${Python3_VERSION}/")
    set(filename "python-${Python3_VERSION}-embed-amd64.zip")

    if (NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${filename}" OR NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/python")
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
            FILES ${CMAKE_CURRENT_BINARY_DIR}/python/${PYTHON_FILE}
            DESTINATION "bin"
            COMPONENT "Hyperion"
        )
    endforeach()
endif(ENABLE_EFFECTENGINE)

if (ENABLE_DX)
    # Download DirectX End-User Runtimes (June 2010)
    set(url "https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe")
    if (NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/dx_redist.exe")
        file(DOWNLOAD "${url}" "${CMAKE_CURRENT_BINARY_DIR}/dx_redist.exe"
            STATUS result
        )

        # Check if the download is successful
        list(GET result 0 result_code)
        if (NOT result_code EQUAL 0)
            list(GET result 1 reason)
            message(FATAL_ERROR "Could not download DirectX End-User Runtimes: ${reason}")
        endif()
    endif()

    # Copy DirectX End-User Runtimes to 'hyperion'
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/dx_redist.exe
        DESTINATION "bin"
        COMPONENT "Hyperion"
    )
endif (ENABLE_DX)
