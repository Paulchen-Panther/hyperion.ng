include_guard(GLOBAL)

if(CMAKE_VERSION VERSION_LESS "3.19")
	message(WARNING "Fetching pre-built dependencies requires CMake >= 3.19 (found ${CMAKE_VERSION}), building all dependencies instead")
	return()
endif()

if(APPLE)
	set(_os "macos")
elseif(WIN32)
	set(_os "windows")
elseif(UNIX)
	set(_os "linux")
else()
	message(WARNING "Unsupported operating system - cannot fetch pre-built dependencies")
	return()
endif()

if(CPACK_SYSTEM_PROCESSOR)
	string(TOLOWER "${CPACK_SYSTEM_PROCESSOR}" _arch_raw)
else()
	string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _arch_raw)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	if(_os STREQUAL "linux")
		# All 32-bit Linux targets (armv6, armv7)
		set(_arch "armhf")
	else()
		message(WARNING "32-bit ${_os} is not supported - cannot fetch pre-built dependencies")
		return()
	endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	if(_arch_raw MATCHES "^(aarch64|arm64)")
		set(_arch "arm64")
	elseif(APPLE OR WIN32)
		set(_arch "x64")
	else()
		set(_arch "amd64")
	endif()
else()
	message(WARNING "Could not determine pointer size (CMAKE_SIZEOF_VOID_P='${CMAKE_SIZEOF_VOID_P}') - cannot fetch pre-built dependencies")
	return()
endif()

file(READ "${CMAKE_SOURCE_DIR}/PreBuiltSpecs.json" _deps)

string(TOLOWER "${CMAKE_BUILD_TYPE}" _type)
if(NOT _type)
	set(_type "release")
endif()

string(JSON _hash ERROR_VARIABLE _err GET "${_deps}" hashes ${_os} ${_arch} ${_type})
if(_err)
	message(WARNING "No pre-built dependencies for ${_os}-${_arch}-${_type}, building all dependencies instead")
	return()
endif()
string(JSON _qt      GET "${_deps}" hashes ${_os} ${_arch} qt_version)
string(JSON _version GET "${_deps}" version)
string(JSON _url     GET "${_deps}" baseUrl)

# The directory name contains version, Qt and build type -> a new release is fetched automatically
set(_name "${_os}-${_arch}-qt${_qt}-${_type}-${_version}")
set(_dir "${CMAKE_BINARY_DIR}/.deps/${_name}")

if(NOT EXISTS "${_dir}")
	set(_archive "${_dir}.tar.gz")

	message(STATUS "Downloading pre-built dependencies: ${_name}")
	file(DOWNLOAD "${_url}/${_version}/${_name}.tar.gz" "${_archive}"
		EXPECTED_HASH SHA256=${_hash}
		INACTIVITY_TIMEOUT 60
		STATUS _status
	)
	list(GET _status 0 _code)
	if(NOT _code EQUAL 0)
		list(GET _status 1 _msg)
		file(REMOVE "${_archive}")
		message(WARNING "Download of ${_name} failed (${_msg}), building all dependencies instead")
		return()
	endif()

	file(REMOVE_RECURSE "${_dir}.tmp")
	file(MAKE_DIRECTORY "${_dir}.tmp")
	execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf "${_archive}"
		WORKING_DIRECTORY "${_dir}.tmp"
		RESULT_VARIABLE _code
	)

	file(REMOVE "${_archive}")
	if(NOT _code EQUAL 0)
		file(REMOVE_RECURSE "${_dir}.tmp")
		message(WARNING "Extracting ${_name} failed, building all dependencies instead")
		return()
	endif()

	# flatc and protoc must be executable (not guaranteed by the archive)
	file(GLOB _flat_proto "${_dir}.tmp/bin/*")
	if(_flat_proto AND NOT WIN32)
		file(CHMOD ${_flat_proto} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	endif()

	file(RENAME "${_dir}.tmp" "${_dir}")
endif()

set(PRE_BUILT_DEPS_DIR "${_dir}")
message(STATUS "Pre-built dependencies: ${PRE_BUILT_DEPS_DIR}")
