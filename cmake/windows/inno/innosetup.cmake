# Inno Setup compiler lookup: Inno Setup 7 is preferred, Inno Setup 6.3 or newer is the minimum
# (the .iss script enforces the version). A compiler found in PATH wins over the paths below,
# use -DINNO_SETUP_COMPILER=<path to ISCC.exe> to select a specific one.
find_program(INNO_SETUP_COMPILER
	NAMES
		ISCC iscc
	PATHS
		"C:/Program Files/Inno Setup 7"
		"C:/Program Files (x86)/Inno Setup 7"
		"$ENV{LOCALAPPDATA}/Programs/Inno Setup 7"
		"C:/Program Files (x86)/Inno Setup 6"
		"C:/Program Files/Inno Setup 6"
		"$ENV{LOCALAPPDATA}/Programs/Inno Setup 6"
)

if(NOT INNO_SETUP_COMPILER)
	message(FATAL_ERROR "Inno Setup Compiler (6.3 or newer) not found. You won't be able to build setup files.")
else()
	message(STATUS "Using Inno Setup Compiler: ${INNO_SETUP_COMPILER}")

	configure_file("${CMAKE_CURRENT_LIST_DIR}/windows.iss.in" "${CPACK_TOPLEVEL_DIRECTORY}/inno/windows.iss" @ONLY)
	configure_file("${CMAKE_CURRENT_LIST_DIR}/modpath.iss" "${CPACK_TOPLEVEL_DIRECTORY}/inno/modpath.iss" COPYONLY)
	file(COPY "${CMAKE_CURRENT_LIST_DIR}/gfx" DESTINATION "${CPACK_TOPLEVEL_DIRECTORY}/inno/")
	file(COPY "${CMAKE_CURRENT_LIST_DIR}/i18n" DESTINATION "${CPACK_TOPLEVEL_DIRECTORY}/inno/")

	unset(ISETUP_ARGS)
	foreach(COMPONENT ${CPACK_COMPONENTS_ALL})
		if(NOT ${COMPONENT} STREQUAL dependencies)
			list(APPEND ISETUP_ARGS "/d${COMPONENT}=1")
		endif()
	endforeach(COMPONENT ${CPACK_COMPONENTS_ALL})

	# 64-bit installer executable, needs Inno Setup 7
	if(CPACK_INNO_64BIT_INSTALLER)
		list(APPEND ISETUP_ARGS "/dInstaller64Bit=1")
	endif()

	if(CPACK_BUILD_CONFIG STREQUAL "Release")
		list(APPEND ISETUP_ARGS "/Q")
	endif()

	EXECUTE_PROCESS(
		COMMAND "${INNO_SETUP_COMPILER}" ${ISETUP_ARGS} "${CPACK_TOPLEVEL_DIRECTORY}/inno/windows.iss"
		ENCODING UTF-8
		COMMAND_ERROR_IS_FATAL ANY
		WORKING_DIRECTORY ${CPACK_TEMPORARY_DIRECTORY}
	)
endif()
