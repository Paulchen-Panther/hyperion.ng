add_library(python
	${CMAKE_SOURCE_DIR}/include/python/PythonInit.h
	${CMAKE_SOURCE_DIR}/include/python/PythonProgram.h
	${CMAKE_SOURCE_DIR}/include/python/PythonUtils.h
	${CMAKE_SOURCE_DIR}/libsrc/python/PythonInit.cpp
	${CMAKE_SOURCE_DIR}/libsrc/python/PythonProgram.cpp
)

if(NOT CMAKE_VERSION VERSION_LESS "3.12")
	find_package(Python3 COMPONENTS Interpreter Development QUIET REQUIRED)
	set(PYTHON_VERSION ${Python3_VERSION} PARENT_SCOPE)

else()
	find_package(PythonInterp 3.5 QUIET REQUIRED)
	set(PYTHON_VERSION ${PYTHON_VERSION_STRING} PARENT_SCOPE)
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
	PYTHON_VERSION_MAJOR=$<$<BOOL:${Python3_FOUND}>:${Python3_VERSION_MAJOR}>$<$<BOOL:${PYTHONLIBS_FOUND}>:${PYTHON_VERSION_MAJOR}>
	PYTHON_VERSION_MINOR=$<$<BOOL:${Python3_FOUND}>:${Python3_VERSION_MINOR}>$<$<BOOL:${PYTHONLIBS_FOUND}>:${PYTHON_VERSION_MINOR}>
)
