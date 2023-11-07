add_library(python
	${CMAKE_SOURCE_DIR}/include/python/PythonInit.h
	${CMAKE_SOURCE_DIR}/include/python/PythonProgram.h
	${CMAKE_SOURCE_DIR}/include/python/PythonUtils.h
	${CMAKE_SOURCE_DIR}/libsrc/python/PythonInit.cpp
	${CMAKE_SOURCE_DIR}/libsrc/python/PythonProgram.cpp
)

# Include the python directory. Also include the parent (which is for example /usr/include)
# which may be required when it is not includes by the (cross-) compiler by default.
if(NOT CMAKE_VERSION VERSION_LESS "3.12")
	find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
	target_link_libraries(python ${Python3_LIBRARIES})
	target_include_directories(python PUBLIC ${Python3_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS}/..)
	add_compile_definitions(PYTHON_VERSION_MAJOR=${Python3_VERSION_MAJOR})
	add_compile_definitions(PYTHON_VERSION_MINOR=${Python3_VERSION_MINOR})
else()
	find_package (PythonLibs ${PYTHON_VERSION_STRING} EXACT) # Maps PythonLibs to the PythonInterp version of the main cmake
	target_link_libraries(python ${PYTHON_LIBRARIES})
	target_include_directories(python PUBLIC ${PYTHON_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS}/..)
	add_definitions(-DPYTHON_VERSION_MAJOR=${PYTHON_VERSION_MAJOR})
	add_definitions(-DPYTHON_VERSION_MINOR=${PYTHON_VERSION_MINOR})
endif()

target_link_libraries(python
	effectengine
	hyperion-utils
)
