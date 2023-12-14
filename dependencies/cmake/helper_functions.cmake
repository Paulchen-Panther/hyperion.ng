include(CMakeParseArguments)

# Put the TARGET's build interface include directory into the OUTPUT var
function(get_build_interface_include_directory)
	cmake_parse_arguments(arg "" "TARGET;OUTPUT" "" ${ARGN})

	if(NOT arg_TARGET)
		message(SEND_ERROR "TARGET is a required argument")
		return()
	endif()

	if(NOT arg_OUTPUT)
		message(SEND_ERROR "OUTPUT is a required argument")
		return()
	endif()

	if(NOT TARGET ${arg_TARGET})
		set(${arg_OUTPUT} "${arg_TARGET}-NOTFOUND" PARENT_SCOPE)
		return()
	endif()

	set(_output "")
	get_target_property(_output ${arg_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
	foreach(include ${_output})
		if(include MATCHES "\\$<BUILD_INTERFACE:([^,>]+)>")
			set(_output "${CMAKE_MATCH_1}")
		endif()
	endforeach()

	set(${arg_OUTPUT} "${_output}" PARENT_SCOPE)
endfunction()
