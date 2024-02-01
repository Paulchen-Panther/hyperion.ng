macro(addIndent text)
	if(${CMAKE_VERSION} VERSION_GREATER "3.16.0")
		list(APPEND CMAKE_MESSAGE_INDENT ${text})
	endif()
endmacro()

macro(removeIndent)
	if(${CMAKE_VERSION} VERSION_GREATER "3.16.0")
		list(POP_BACK CMAKE_MESSAGE_INDENT)
	endif()
endmacro()

# Macro to get path of first sub dir of a dir, used for MAC OSX lib/header searching
macro(FIRSTSUBDIR result curdir)
  file(GLOB children RELATIVE ${curdir} ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
	if(IS_DIRECTORY ${curdir}/${child})
	  list(APPEND dirlist "${curdir}/${child}")
		break()
	endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

# Prints configuration summary
macro(PrintConfigurationSummary)
	message(STATUS "************** Summary **************")
	message(STATUS "General:")
	addIndent(" - ")
	message(STATUS "CMake version                : ${CMAKE_VERSION}")
	message(STATUS "System                       : ${CMAKE_SYSTEM_NAME}")
	message(STATUS "C Compiler                   : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
	message(STATUS "C++ Compiler                 : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
	message(STATUS "Using ccache if found        : ${USE_CCACHE}")
	message(STATUS "Build type                   : ${CMAKE_BUILD_TYPE}")
	message(STATUS "Target platform              : ${PLATFORM}")
	if(ENABLE_JSONCHECKS OR ENABLE_EFFECTENGINE AND PYTHON_VERSION)
		message(STATUS "Python version               : ${PYTHON_VERSION}")
	endif()
	if(ENABLE_FLATBUF_SERVER OR ENABLE_FLATBUF_CONNECT AND (TARGET flatbuffers))
		get_target_property(FLATBUFFERS_VERSION flatbuffers FLATBUFFERS_VERSION_PROPERTY)
		if(FLATBUFFERS_VERSION)
			message(STATUS "FlatBuffers version          : ${FLATBUFFERS_VERSION}")
		endif()
	endif()
	if(ENABLE_DEV_NETWORK AND TARGET mbedtls)
		get_target_property(MBEDTLS_VERSION mbedtls MBEDTLS_VERSION_PROPERTY)
		if(MBEDTLS_VERSION)
			message(STATUS "mbedTLS version              : ${MBEDTLS_VERSION}")
		endif()
	endif()
	if(ENABLE_DEV_USB_HID AND TARGET libusb)
		get_target_property(LIBUSB_VERSION libusb LIBUSB_VERSION_PROPERTY)
		if(LIBUSB_VERSION)
			message(STATUS "LibUSB version               : ${LIBUSB_VERSION}")
		endif()
	endif()
	message(STATUS "Qt version                   : ${QT_VERSION}")
	if(DEFINED QTDIR)
		message(STATUS "QTDIR used                   : ${CMAKE_PREFIX_PATH}")
	endif()
	if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		message(STATUS "Windows SDK                  : ${WINDOWSSDK_LATEST_DIR} ${WINDOWSSDK_LATEST_NAME}")
		message(STATUS "MSVC version                 : ${MSVC_VERSION}")
	endif()
	removeIndent()

	message(STATUS "Hyperion info:")
	addIndent(" - ")
	message(STATUS "Current version              : ${HYPERION_VERSION}")
	message(STATUS "Build                        : ${VERSION_ID} (${BUILD_ID})")
	if(HYPERION_LIGHT)
		set(HYPERION_LIGHT_NOTES "(Hyperion is build with a reduced set of functionality.)")
	endif()
	message(STATUS "HYPERION_LIGHT               : ${HYPERION_LIGHT} ${HYPERION_LIGHT_NOTES}")
	removeIndent()

	message(STATUS "Grabber options:")
	addIndent(" - ")
	message(STATUS "ENABLE_AMLOGIC               : ${ENABLE_AMLOGIC}")
	message(STATUS "ENABLE_DISPMANX              : ${ENABLE_DISPMANX}")
	message(STATUS "ENABLE_DX                    : ${ENABLE_DX}")
	message(STATUS "ENABLE_FB                    : ${ENABLE_FB}")
	message(STATUS "ENABLE_MF                    : ${ENABLE_MF}")
	message(STATUS "ENABLE_OSX                   : ${ENABLE_OSX}")
	message(STATUS "ENABLE_QT                    : ${ENABLE_QT}")
	message(STATUS "ENABLE_V4L2                  : ${ENABLE_V4L2}")
	message(STATUS "ENABLE_X11                   : ${ENABLE_X11}")
	message(STATUS "ENABLE_XCB                   : ${ENABLE_XCB}")
	message(STATUS "ENABLE_AUDIO                 : ${ENABLE_AUDIO}")
	removeIndent()

	message(STATUS "Input options:")
	addIndent(" - ")
	message(STATUS "ENABLE_BOBLIGHT_SERVER       : ${ENABLE_BOBLIGHT_SERVER}")
	message(STATUS "ENABLE_CEC                   : ${ENABLE_CEC}")
	message(STATUS "ENABLE_FLATBUF_SERVER        : ${ENABLE_FLATBUF_SERVER}")
	message(STATUS "ENABLE_PROTOBUF_SERVER       : ${ENABLE_PROTOBUF_SERVER}")
	removeIndent()

	message(STATUS "Output options:")
	addIndent(" - ")
	message(STATUS "ENABLE_FORWARDER             : ${ENABLE_FORWARDER}")
	message(STATUS "ENABLE_FLATBUF_CONNECT       : ${ENABLE_FLATBUF_CONNECT}")
	removeIndent()

	message(STATUS "LED-Device options:")
	addIndent(" - ")
	message(STATUS "ENABLE_DEV_NETWORK           : ${ENABLE_DEV_NETWORK}")
	message(STATUS "ENABLE_DEV_SERIAL            : ${ENABLE_DEV_SERIAL}")
	message(STATUS "ENABLE_DEV_SPI               : ${ENABLE_DEV_SPI}")
	message(STATUS "ENABLE_DEV_TINKERFORGE       : ${ENABLE_DEV_TINKERFORGE}")
	message(STATUS "ENABLE_DEV_USB_HID           : ${ENABLE_DEV_USB_HID}")
	message(STATUS "ENABLE_DEV_WS281XPWM         : ${ENABLE_DEV_WS281XPWM}")
	removeIndent()

	message(STATUS "Services options:")
	addIndent(" - ")
	message(STATUS "ENABLE_EFFECTENGINE          : ${ENABLE_EFFECTENGINE}")
	message(STATUS "ENABLE_MDNS                  : ${ENABLE_MDNS}")
	removeIndent()

	message(STATUS "Standalone binaries:")
	addIndent(" - ")
	message(STATUS "ENABLE_REMOTE_CTL            : ${ENABLE_REMOTE_CTL}")
	message(STATUS "ENABLE_AMLOGIC_EXT           : ${ENABLE_AMLOGIC_EXT}")
	message(STATUS "ENABLE_V4L2_EXT              : ${ENABLE_V4L2_EXT}")
	message(STATUS "ENABLE_X11_EXT               : ${ENABLE_X11_EXT}")
	message(STATUS "ENABLE_XCB_EXT               : ${ENABLE_XCB_EXT}")
	message(STATUS "ENABLE_DISPMANX_EXT          : ${ENABLE_DISPMANX_EXT}")
	message(STATUS "ENABLE_FB_EXT                : ${ENABLE_FB_EXT}")
	message(STATUS "ENABLE_QT_EXT                : ${ENABLE_QT_EXT}")
	message(STATUS "ENABLE_OSX_EXT               : ${ENABLE_OSX_EXT}")
	removeIndent()

	message(STATUS "3rd party:")
	addIndent(" - ")
	if(ENABLE_MDNS)
		message(STATUS "USE_SYSTEM_QMDNS_LIBS        : ${USE_SYSTEM_QMDNS_LIBS}")
	endif()
	if(ENABLE_FLATBUF_SERVER OR ENABLE_FLATBUF_CONNECT)
		message(STATUS "USE_SYSTEM_FLATBUFFERS_LIBS  : ${USE_SYSTEM_FLATBUFFERS_LIBS}")
	endif()
	if(ENABLE_PROTOBUF_SERVER)
		message(STATUS "USE_SYSTEM_PROTO_LIBS        : ${USE_SYSTEM_PROTO_LIBS}")
	endif()
	if(ENABLE_DEV_NETWORK)
		message(STATUS "USE_SYSTEM_MBEDTLS_LIBS      : ${USE_SYSTEM_MBEDTLS_LIBS}")
	endif()
	removeIndent()

	message(STATUS "Tests:")
	addIndent(" - ")
	message(STATUS "ENABLE_TESTS                 : ${ENABLE_TESTS}")
	removeIndent()

	message(STATUS "Advanced features:")
	addIndent(" - ")
	message(STATUS "ENABLE_DEPLOY_DEPENDENCIES   : ${ENABLE_DEPLOY_DEPENDENCIES}")
	message(STATUS "ENABLE_JSONCHECKS            : ${ENABLE_JSONCHECKS}")
	message(STATUS "ENABLE_EXPERIMENTAL          : ${ENABLE_EXPERIMENTAL}")
	message(STATUS "ENABLE_PROFILER              : ${ENABLE_PROFILER}")
	removeIndent()

	message(STATUS "*************************************")
endmacro()
