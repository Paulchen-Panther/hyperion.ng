include(GetPrerequisites)

set(SYSTEM_LIBS_SKIP
    "libatomic"
    "libc"
    "libdbus"
    "libdl"
    "libexpat"
    "libfontconfig"
    "libgcc_s"
    "libgcrypt"
    "libglib"
    "libglib-2"
    "libgpg-error"
    "liblz4"
    "liblzma"
    "libm"
    "libpcre"
    "libpcre2"
    "libpthread"
    "librt"
    "libstdc++"
    "libsystemd"
    "libudev"
    "libusb"
    "libusb-1"
    "libutil"
    "libuuid"
    "libz"
)

if (ENABLE_DISPMANX)
    list(APPEND SYSTEM_LIBS_SKIP "libcec")
endif()

# Extract dependencies ignoring the system ones
get_prerequisites(${TARGET_FILE} DEPENDENCIES 0 1 "" "")

message(STATUS "Dependencies for target file: ${DEPENDENCIES}")

# Append symlink and non-symlink dependencies to the list
set(PREREQUISITE_LIBS "")
foreach(DEPENDENCY ${DEPENDENCIES})
    get_filename_component(resolved ${DEPENDENCY} NAME_WE)
    list(FIND SYSTEM_LIBS_SKIP ${resolved} _index)

    if (${_index} GREATER -1)
        continue() # Skip system libraries
    else()
        gp_resolve_item("${TARGET_FILE}" "${DEPENDENCY}" "" "" resolved_file)
        get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
        gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
        get_filename_component(file_canonical ${resolved_file} REALPATH)
        gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
    endif()
endforeach()

# Append the OpenSSL library to the list
find_package(OpenSSL 1.0.0 REQUIRED)
if (OPENSSL_FOUND)
    foreach(openssl_lib ${OPENSSL_LIBRARIES})
        get_prerequisites(${openssl_lib} openssl_deps 0 1 "" "")

        foreach(openssl_dep ${openssl_deps})
            get_filename_component(resolved ${openssl_dep} NAME_WE)
            list(FIND SYSTEM_LIBS_SKIP ${resolved} _index)
            if (${_index} GREATER -1)
                continue() # Skip system libraries
            else()
                gp_resolve_item("${openssl_lib}" "${openssl_dep}" "" "" resolved_file)
                get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
                gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
                get_filename_component(file_canonical ${resolved_file} REALPATH)
                gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
            endif()
        endforeach()

        gp_append_unique(PREREQUISITE_LIBS ${openssl_lib})
        get_filename_component(file_canonical ${openssl_lib} REALPATH)
        gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
    endforeach()
else()
    message( WARNING "OpenSSL NOT found (https webserver will not work)")
endif(OPENSSL_FOUND)

# Detect the Qt plugin directory, source: https://github.com/lxde/lxqt-qtplugin/blob/master/src/CMakeLists.txt
if ( TARGET Qt${QT_VERSION_MAJOR}::qmake )
    get_target_property(QT_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    execute_process(
        COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
        OUTPUT_VARIABLE QT_PLUGINS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

# Copy Qt plugins to 'share/hyperion/lib'
if (QT_PLUGINS_DIR)
    foreach(PLUGIN "platforms" "sqldrivers" "imageformats" "tls" "wayland-shell-integration")
        if (EXISTS ${QT_PLUGINS_DIR}/${PLUGIN})
            file(GLOB files "${QT_PLUGINS_DIR}/${PLUGIN}/*.so")
            foreach(file ${files})
                get_prerequisites(${file} PLUGINS 0 1 "" "")
                foreach(DEPENDENCY ${PLUGINS})
                    get_filename_component(resolved ${DEPENDENCY} NAME_WE)
                    list(FIND SYSTEM_LIBS_SKIP ${resolved} _index)
                    if (${_index} GREATER -1)
                        continue() # Skip system libraries
                    else()
                        gp_resolve_item("${file}" "${DEPENDENCY}" "" "" resolved_file)
                        get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
                        gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
                        get_filename_component(file_canonical ${resolved_file} REALPATH)
                        gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
                    endif()
                endforeach()

                install(
                    FILES ${file}
                    DESTINATION "share/hyperion/lib/${PLUGIN}"
                    COMPONENT "Hyperion"
                )
            endforeach()
        endif()
    endforeach()
endif(QT_PLUGINS_DIR)

# Create a qt.conf file in 'share/hyperion/bin' to override hard-coded search paths in Qt plugins
file(WRITE "${CMAKE_BINARY_DIR}/qt.conf" "[Paths]\nPlugins=../lib/\n")
install(
    FILES "${CMAKE_BINARY_DIR}/qt.conf"
    DESTINATION "share/hyperion/bin"
    COMPONENT "Hyperion"
)

# Copy dependencies to 'share/hyperion/lib'
foreach(PREREQUISITE_LIB ${PREREQUISITE_LIBS})
    install(
        FILES ${PREREQUISITE_LIB}
        DESTINATION "share/hyperion/lib"
        COMPONENT "Hyperion"
    )
endforeach()

if(ENABLE_EFFECTENGINE)
    # Detect the Python version and modules directory
    if (NOT CMAKE_VERSION VERSION_LESS "3.12")
        find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
        set(PYTHON_VERSION_MAJOR_MINOR "${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")
        set(PYTHON_MODULES_DIR ${Python3_STDLIB})
    else()
        find_package (PythonLibs ${PYTHON_VERSION_STRING} EXACT)
        set(PYTHON_VERSION_MAJOR_MINOR "${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}")
        set(PYTHON_MODULES_DIR ${Python_STDLIB})
    endif()

    # Copy Python modules to 'share/hyperion/lib/pythonMAJOR.MINOR' and ignore the unnecessary stuff listed below
    if (PYTHON_MODULES_DIR)

        install(
            DIRECTORY ${PYTHON_MODULES_DIR}/
            DESTINATION "share/hyperion/lib/python${PYTHON_VERSION_MAJOR_MINOR}"
            COMPONENT "Hyperion"
            PATTERN "*.pyc"                                 EXCLUDE # compiled bytecodes
            PATTERN "__pycache__"                           EXCLUDE # any cache
            PATTERN "config-${PYTHON_VERSION_MAJOR_MINOR}*" EXCLUDE # static libs
            PATTERN "lib2to3"                               EXCLUDE # automated Python 2 to 3 code translation
            PATTERN "tkinter"                               EXCLUDE # Tk interface
            PATTERN "turtle.py"                             EXCLUDE # Tk demo
            PATTERN "test"                                  EXCLUDE # unittest module
            PATTERN "sitecustomize.py"                      EXCLUDE # site-specific configs
        )
    endif(PYTHON_MODULES_DIR)
endif(ENABLE_EFFECTENGINE)
