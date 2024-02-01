# - Try to find libusb-1.0
# Once done this will define
#
#  LIBUSB_1_FOUND - system has libusb
#  LIBUSB_1_INCLUDE_DIRS - the libusb include directory
#  LIBUSB_1_LIBRARIES - Link these to use libusb
#  LIBUSB_1_DEFINITIONS - Compiler switches required for using libusb
#
#  Adapted from cmake-modules Google Code project
#
#  Copyright (c) 2006 Andreas Schneider <mail@cynapses.org>
#
#  (Changes for libusb) Copyright (c) 2008 Kyle Machulis <kyle@nonpolynomial.com>
#
# Redistribution and use is allowed according to the terms of the New BSD license.
#
# CMake-Modules Project New BSD License
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of the CMake-Modules Project nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

set(LIBUSB_ROOT_DIR "${LIBUSB_ROOT_DIR}" CACHE PATH "Root directory to search for libusb")

if(TARGET libusb)
	# in cache already
	set(LIBUSB_FOUND TRUE)
else()
	find_path(LIBUSB_INCLUDE_DIR
		NAMES
			libusb.h
		PATHS
			/usr
			/usr/local
			/opt/local
			/opt/homebrew
			/sw
		HINTS
			${LIBUSB_ROOT_DIR}
		PATH_SUFFIXES
			include/libusb-1.0
			include
			libusb-1.0
	)

	find_library(LIBUSB_LIBRARY
		NAMES
			libusb-1.0
			usb-1.0
			usb
		PATHS
			/usr
			/usr/local
			/opt/local
			/opt/homebrew
			/sw
		HINTS
			${LIBUSB_ROOT_DIR}
		PATH_SUFFIXES
			lib
	)

	if(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARY)
		set(LIBUSB_FOUND TRUE)
	endif()

	if(LIBUSB_FOUND)
		if(NOT LIBUSB_FIND_QUIETLY)
			message(STATUS "Found libusb-1.0:")
			message(STATUS " - Includes: ${LIBUSB_INCLUDE_DIR}")
			message(STATUS " - Libraries: ${LIBUSB_LIBRARY}")
		endif()

		add_library(libusb UNKNOWN IMPORTED GLOBAL)
		set_target_properties(libusb PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION "${LIBUSB_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB_INCLUDE_DIR}"
		)

		# libusb version detection from: https://github.com/matwey/libopenvizsla/blob/master/cmake/FindLibUSB1.cmake
		# modified by Hyperion Project
		if(NOT CMAKE_CROSSCOMPILING)
			file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.c"
				"#include <libusb.h>
				#include <stdio.h>
				int main() {
					const struct libusb_version* v=libusb_get_version();
					printf(\"%d.%d.%d%s\",v->major,v->minor,v->micro,v->rc);
					return 0;
				}"
			)

			try_run(RUN_RESULT COMPILE_RESULT
				${CMAKE_BINARY_DIR}
				${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.c
				CMAKE_FLAGS -DINCLUDE_DIRECTORIES:STRING=${LIBUSB1_INCLUDE_DIRS} -DLINK_LIBRARIES:STRING=${LIBUSB1_LIBRARIES}
				RUN_OUTPUT_VARIABLE LIBUSB_VERSION
			)

			if(RUN_RESULT EQUAL 0 AND COMPILE_RESULT)
				define_property(TARGET PROPERTY LIBUSB_VERSION_PROPERTY
					BRIEF_DOCS "Custom LibUSB version target property."
					FULL_DOCS "Custom LibUSB version target property."
				)

				set_target_properties(libusb PROPERTIES
					LIBUSB_VERSION_PROPERTY "${LIBUSB_VERSION}"
				)
			endif()

			unset(RUN_RESULT)
			unset(COMPILE_RESULT)
		endif()
	else()
		message(FATAL_ERROR "Could not find libusb")
	endif()

	mark_as_advanced(LIBUSB_INCLUDE_DIR LIBUSB_LIBRARY)
endif()
