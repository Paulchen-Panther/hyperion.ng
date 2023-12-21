get_target_property(QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
execute_process(
    COMMAND ${QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE QT_PLUGIN_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
get_target_property(TARGET_BUNDLE_NAME ${PROJECT_NAME} OUTPUT_NAME)

string(CONFIGURE [[
    set(TARGET_FILE ${TARGET_FILE})
    set(TARGET_BUNDLE_NAME ${TARGET_BUNDLE_NAME}.app)
    set(PLUGIN_DIR ${QT_PLUGIN_DIR})
    set(BUILD_DIR ${CMAKE_BINARY_DIR})
    set(ENABLE_EFFECTENGINE ${ENABLE_EFFECTENGINE})
]] setVariables)
install(CODE ${setVariables} COMPONENT "Hyperion")

install(CODE [[
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES ${TARGET_FILE}
        RESOLVED_DEPENDENCIES_VAR resolved_deps
        UNRESOLVED_DEPENDENCIES_VAR unresolved_deps
    )

    message(STATUS "resolved_deps = ${resolved_deps}")

    foreach(dependency ${resolved_deps})
        string(FIND ${dependency} "dylib;dSYM" _index)
        if (${_index} GREATER -1)
            message(STATUS "Frameworks = ${dependency}")
            file(INSTALL
                FILES "${dependency}"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/${TARGET_BUNDLE_NAME}/Contents/Frameworks"
                TYPE SHARED_LIBRARY
            )
        else()
            message(STATUS "lib = ${dependency}")
            file(INSTALL
                FILES "${dependency}"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/${TARGET_BUNDLE_NAME}/Contents/lib"
                TYPE SHARED_LIBRARY
                FOLLOW_SYMLINK_CHAIN
            )
        endif()
    endforeach()

    list(LENGTH unresolved_deps unresolved_length)
    if("${unresolved_length}" GREATER 0)
        message(STATUS "The following unresolved dependencies were discovered: ${unresolved_deps}")
    endif()

    foreach(PLUGIN "platforms" "sqldrivers" "imageformats" "tls")
        if(EXISTS ${PLUGIN_DIR}/${PLUGIN})
            file(GLOB files "${PLUGIN_DIR}/${PLUGIN}/*")
            foreach(file ${files})
                    file(GET_RUNTIME_DEPENDENCIES
                    EXECUTABLES ${file}
                    RESOLVED_DEPENDENCIES_VAR PLUGINS
                    UNRESOLVED_DEPENDENCIES_VAR unresolved_deps
                    )

                    foreach(DEPENDENCY ${PLUGINS})
                            file(INSTALL
                                DESTINATION "${CMAKE_INSTALL_PREFIX}/${TARGET_BUNDLE_NAME}/Contents/lib"
                                TYPE SHARED_LIBRARY
                                FILES ${DEPENDENCY}
                                FOLLOW_SYMLINK_CHAIN
                            )
                    endforeach()

                    get_filename_component(singleQtLib ${file} NAME)
                    list(APPEND QT_PLUGINS "${CMAKE_INSTALL_PREFIX}/${TARGET_BUNDLE_NAME}/Contents/plugins/${PLUGIN}/${singleQtLib}")
                    file(INSTALL
                        FILES ${file}
                        DESTINATION "${CMAKE_INSTALL_PREFIX}/${TARGET_BUNDLE_NAME}/Contents/plugins/${PLUGIN}"
                        TYPE SHARED_LIBRARY
                    )
            endforeach()

            list(LENGTH unresolved_deps unresolved_length)
            if("${unresolved_length}" GREATER 0)
                message(STATUS "The following unresolved dependencies were discovered: ${unresolved_deps}")
            endif()
        endif()
    endforeach()

    include(BundleUtilities)
    fixup_bundle("${CMAKE_INSTALL_PREFIX}/${TARGET_BUNDLE_NAME}" "${QT_PLUGINS}" "${CMAKE_INSTALL_PREFIX}/${TARGET_BUNDLE_NAME}/Contents/lib" IGNORE_ITEM "python;python3;Python;Python3;.Python;.Python3")

    if(ENABLE_EFFECTENGINE)
        # Detect the Python version and modules directory
        if(NOT CMAKE_VERSION VERSION_LESS "3.12")
            find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
            set(PYTHON_VERSION_MAJOR_MINOR "${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")
            set(PYTHON_MODULES_DIR ${Python3_STDLIB})
        else()
            find_package (PythonLibs ${PYTHON_VERSION_STRING} EXACT)
            set(PYTHON_VERSION_MAJOR_MINOR "${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}")
            set(PYTHON_MODULES_DIR ${Python_STDLIB})
        endif()

        MESSAGE("Add Python ${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR} to bundle")
        MESSAGE("PYTHON_MODULES_DIR: ${PYTHON_MODULES_DIR}")

        # Copy Python modules to '/../Frameworks/Python.framework/Versions/Current/lib/PythonMAJOR.MINOR' and ignore the unnecessary stuff listed below
        if (PYTHON_MODULES_DIR)
            file(
                COPY ${PYTHON_MODULES_DIR}/
                DESTINATION "${CMAKE_INSTALL_PREFIX}/${TARGET_BUNDLE_NAME}/Contents/Frameworks/Python.framework/Versions/Current/lib/python${PYTHON_VERSION_MAJOR_MINOR}"
                PATTERN "*.pyc"                                 EXCLUDE # compiled bytecodes
                PATTERN "__pycache__"                           EXCLUDE # any cache
                PATTERN "config-${PYTHON_VERSION_MAJOR_MINOR}*" EXCLUDE # static libs
                PATTERN "lib2to3"                               EXCLUDE # automated Python 2 to 3 code translation
                PATTERN "tkinter"                               EXCLUDE # Tk interface
                PATTERN "turtledemo"                            EXCLUDE # Tk demo folder
                PATTERN "turtle.py"                             EXCLUDE # Tk demo file
                PATTERN "test"                                  EXCLUDE # unittest module
                PATTERN "sitecustomize.py"                      EXCLUDE # site-specific configs
            )
        endif(PYTHON_MODULES_DIR)
    endif(ENABLE_EFFECTENGINE)

    file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/${TARGET_BUNDLE_NAME}/Contents/lib")
    file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/share")
]] COMPONENT "Hyperion")
