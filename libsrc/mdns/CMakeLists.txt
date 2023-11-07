add_library(mdns
	${CMAKE_SOURCE_DIR}/include/mdns/MdnsServiceRegister.h
	${CMAKE_SOURCE_DIR}/include/mdns/MdnsBrowser.h
	${CMAKE_SOURCE_DIR}/include/mdns/MdnsProvider.h
	${CMAKE_SOURCE_DIR}/libsrc/mdns/MdnsBrowser.cpp
	${CMAKE_SOURCE_DIR}/libsrc/mdns/MdnsProvider.cpp
)

target_link_libraries(mdns hyperion ${QMDNS_LIBRARIES})
target_include_directories(mdns PUBLIC ${QMDNS_INCLUDE_DIR})
