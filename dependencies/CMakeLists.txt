list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(ENABLE_DEV_USB_HID)
	add_subdirectory(build/hidapi)
endif()

if(ENABLE_DEV_TINKERFORGE)
	add_subdirectory(build/tinkerforge)
endif()

if(ENABLE_DEV_WS281XPWM)
	add_library(ws281x
		external/rpi_ws281x/mailbox.c external/rpi_ws281x/ws2811.c
		external/rpi_ws281x/pwm.c external/rpi_ws281x/dma.c
		external/rpi_ws281x/pcm.c
		external/rpi_ws281x/rpihw.c)
endif()

if(ENABLE_MDNS)
	include(qmdnsengine)
endif()

if(ENABLE_FLATBUF_SERVER OR ENABLE_FLATBUF_CONNECT)
	include(flatbuffers)
endif()

if(ENABLE_PROTOBUF_SERVER)
	include(protobuf)
endif()

if(ENABLE_DEV_NETWORK)
	include(mbedtls)
endif(ENABLE_DEV_NETWORK)
