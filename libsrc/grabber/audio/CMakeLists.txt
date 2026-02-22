if(WIN32)
	add_definitions(-DUNICODE -D_UNICODE)
	set(AUDIO_GRABBER_SOURCES
		${CMAKE_SOURCE_DIR}/include/grabber/audio/AudioGrabberWindows.h
		${CMAKE_SOURCE_DIR}/libsrc/grabber/audio/AudioGrabberWindows.cpp
	)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(AUDIO_GRABBER_SOURCES
		${CMAKE_SOURCE_DIR}/include/grabber/audio/AudioGrabberLinux.h
		${CMAKE_SOURCE_DIR}/libsrc/grabber/audio/AudioGrabberLinux.cpp
	)
endif()

if(ENABLE_PROJECTM)
	list(APPEND AUDIO_GRABBER_SOURCES
		${CMAKE_SOURCE_DIR}/include/grabber/audio/ProjectMWrapper.h
		${CMAKE_SOURCE_DIR}/libsrc/grabber/audio/ProjectMWrapper.cpp
	)
endif()

add_library(audio-grabber
	${CMAKE_SOURCE_DIR}/include/grabber/audio/AudioGrabber.h
	${CMAKE_SOURCE_DIR}/include/grabber/audio/AudioWrapper.h
	${CMAKE_SOURCE_DIR}/libsrc/grabber/audio/AudioGrabber.cpp
	${CMAKE_SOURCE_DIR}/libsrc/grabber/audio/AudioWrapper.cpp
	${AUDIO_GRABBER_SOURCES}
)

if(WIN32)
	set(AUDIO_LIBS DSound)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(ALSA REQUIRED)
	find_package(Threads REQUIRED)
	set(AUDIO_LIBS ${ALSA_LIBRARIES} Threads::Threads)
endif()

if(ENABLE_PROJECTM)
	find_package(ProjectM REQUIRED)
	find_package(OpenGL REQUIRED COMPONENTS EGL)
	list(APPEND AUDIO_LIBS projectM::projectM OpenGL::EGL)
	target_compile_definitions(audio-grabber PRIVATE ENABLE_PROJECTM)
endif()

target_link_libraries(audio-grabber
	hyperion
	${AUDIO_LIBS}
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	target_include_directories(audio-grabber PUBLIC ${ALSA_INCLUDE_DIRS})
endif()
