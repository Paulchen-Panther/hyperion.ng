find_package(LibDRM 2.4.101 REQUIRED)

add_library(drm-grabber
	${CMAKE_SOURCE_DIR}/include/grabber/drm/DRMFrameGrabber.h
	${CMAKE_SOURCE_DIR}/include/grabber/drm/DRMWrapper.h
	${CMAKE_SOURCE_DIR}/libsrc/grabber/drm/DRMFrameGrabber.cpp
	${CMAKE_SOURCE_DIR}/libsrc/grabber/drm/DRMWrapper.cpp
)

target_include_directories(drm-grabber
	PUBLIC
	${LIBDRM_INCLUDE_DIRS}
)

target_link_libraries(drm-grabber
	hyperion
	${LIBDRM_LIBRARIES}
)

# Optional: libdrmtap (https://github.com/fxd0h/libdrmtap) is used as a fallback
# capture backend for GPU scanouts (e.g. tiled/compressed Intel/AMD/Nvidia
# framebuffers) that the built-in capture code cannot decode. The grabber
# builds and works without it; only that fallback is unavailable.
find_package(LibDrmTap)
if(LIBDRMTAP_FOUND)
	target_compile_definitions(drm-grabber PUBLIC HAVE_LIBDRMTAP)
	target_include_directories(drm-grabber PUBLIC ${LIBDRMTAP_INCLUDE_DIRS})
	target_link_libraries(drm-grabber ${LIBDRMTAP_LIBRARIES})
else()
	message(STATUS "libdrmtap library not found, DRM grabber will not be able to fall back to it for unsupported (e.g. tiled/compressed) GPU scanouts.")
endif()

