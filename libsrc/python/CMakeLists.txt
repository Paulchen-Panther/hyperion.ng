# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/python)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libsrc/python)

FILE ( GLOB PYTHON_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(python
	${PYTHON_SOURCES}
)

if (NOT CMAKE_VERSION VERSION_LESS "3.12.0")
	find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
	target_include_directories(python PUBLIC ${Python3_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS}/..)
	target_link_libraries(python ${Python3_LIBRARIES})
	add_compile_definitions(PYTHON_VERSION_MAJOR=${Python3_VERSION_MAJOR})
	add_compile_definitions(PYTHON_VERSION_MINOR=${Python3_VERSION_MINOR})
else()
	find_package (PythonLibs ${PYTHON_VERSION_STRING} EXACT) # Maps PythonLibs to the PythonInterp version of the main cmake
	target_include_directories(python PUBLIC ${PYTHON_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS}/..)
	target_link_libraries(python ${PYTHON_LIBRARIES})
	add_definitions(-DPYTHON_VERSION_MAJOR=${PYTHON_VERSION_MAJOR})
	add_definitions(-DPYTHON_VERSION_MINOR=${PYTHON_VERSION_MINOR})
endif()

target_link_libraries(python
	effectengine
	hyperion-utils
)
