add_library(python
	${CMAKE_SOURCE_DIR}/include/python/PythonInit.h
	${CMAKE_SOURCE_DIR}/include/python/PythonProgram.h
	${CMAKE_SOURCE_DIR}/include/python/PythonUtils.h
	${CMAKE_SOURCE_DIR}/libsrc/python/PythonInit.cpp
	${CMAKE_SOURCE_DIR}/libsrc/python/PythonProgram.cpp
)

define_property(TARGET PROPERTY PYTHON_VERSION_PROPERTY
	BRIEF_DOCS "Python version property"
	FULL_DOCS "Python version property"
)

define_property(TARGET PROPERTY PYTHON_STDLIB_LOCATION_PROPERTY
	BRIEF_DOCS "Python stdlib location property"
	FULL_DOCS "Python stdlib location property"
)

if(CMAKE_VERSION VERSION_LESS "3.12")
	find_package(PythonInterp 3.5 QUIET REQUIRED)
	set_target_properties(python PROPERTIES PYTHON_VERSION_PROPERTY ${PYTHON_VERSION_STRING})
	set_target_properties(python PROPERTIES PYTHON_STDLIB_LOCATION_PROPERTY ${Python_STDLIB})
else()
	find_package(Python3 COMPONENTS Interpreter Development QUIET REQUIRED)
	set_target_properties(python PROPERTIES PYTHON_VERSION_PROPERTY ${Python3_VERSION})
	set_target_properties(python PROPERTIES PYTHON_STDLIB_LOCATION_PROPERTY ${Python3_STDLIB})
	set(PYTHON_VERSION_MAJOR ${Python3_VERSION_MAJOR})
	set(PYTHON_VERSION_MINOR ${Python3_VERSION_MINOR})
	set(Python_STDLIB ${Python3_STDLIB})
	set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
endif()

target_link_libraries(python
	effectengine
	hyperion-utils
	${Python3_LIBRARIES}
	${PYTHON_LIBRARIES}
)

target_include_directories(python PUBLIC
	${Python3_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS}/..
	${PYTHON_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS}/..
)

target_compile_definitions(python PRIVATE
	PYTHON_VERSION_MAJOR=${PYTHON_VERSION_MAJOR}
	PYTHON_VERSION_MINOR=${PYTHON_VERSION_MINOR}
)

if(ENABLE_JSONCHECKS)
	execute_process(
		COMMAND ${PYTHON_EXECUTABLE} test/jsonchecks/checkeffects.py effects effects/schema
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE CHECK_EFFECTS_FAILED
	)

	if(${CHECK_EFFECTS_FAILED})
		message(FATAL_ERROR "Check of JSON effect files failed")
	endif()
endif()
