add_subdirectory(hyperiond)

# The following standalone binaries compiled if requested
if(ENABLE_REMOTE_CTL)
	add_subdirectory(hyperion-remote)
endif()

if(ENABLE_AMLOGIC_EXT)
	add_subdirectory(hyperion-aml)
endif()

if(ENABLE_V4L2_EXT)
	add_subdirectory(hyperion-v4l2)
endif()

if(ENABLE_X11_EXT)
	add_subdirectory(hyperion-x11)
endif()

if(ENABLE_XCB_EXT)
	add_subdirectory(hyperion-xcb)
endif()

if(ENABLE_DISPMANX_EXT)
	add_subdirectory(hyperion-dispmanx)
endif()

if(ENABLE_FB_EXT)
	add_subdirectory(hyperion-framebuffer)
endif()

if(ENABLE_QT_EXT)
	add_subdirectory(hyperion-qt)
endif()

if(ENABLE_OSX_EXT)
	add_subdirectory(hyperion-osx)
endif()
