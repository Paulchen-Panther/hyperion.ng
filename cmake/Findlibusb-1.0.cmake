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

set(LIBUSB_1_ROOT_DIR "${LIBUSB_1_ROOT_DIR}" CACHE PATH "Root directory to search for libusb-1")

if(TARGET usb-1.0)
	# in cache already
	set(LIBUSB_FOUND TRUE)
else()
	find_path(LIBUSB_1_INCLUDE_DIR
		NAMES
			libusb.h
		PATHS
			/usr/include
			/usr/local/include
			/opt/local/include
			/sw/include
		HINTS
			${LIBUSB_1_ROOT_DIR}
		PATH_SUFFIXES
			libusb-1.0
	)

	find_library(LIBUSB_1_LIBRARY
		NAMES
			libusb-1.0
			usb-1.0
			usb
		PATHS
			/usr/lib
			/usr/local/lib
			/opt/local/lib
			/sw/lib
		HINTS
			${LIBUSB_1_ROOT_DIR}
	)

	if(LIBUSB_1_INCLUDE_DIR AND LIBUSB_1_LIBRARY)
		set(LIBUSB_1_FOUND TRUE)
	endif()

	if(LIBUSB_1_FOUND)
		if (NOT libusb_1_FIND_QUIETLY)
			message(STATUS "Found libusb-1.0:")
			message(STATUS " - Includes: ${LIBUSB_1_INCLUDE_DIR}")
			message(STATUS " - Libraries: ${LIBUSB_1_LIBRARY}")
		endif()

		add_library(usb-1.0 UNKNOWN IMPORTED)
		set_target_properties(usb-1.0 PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION "${LIBUSB_1_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB_1_INCLUDE_DIR}"
		)
	else()
		if(libusb_1_FIND_REQUIRED)
			message(FATAL_ERROR "Could not find libusb")
		endif()
	endif()
endif()
