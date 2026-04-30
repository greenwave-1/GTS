# Alternative build file
# UNSUPPORTED, mainly here so that my IDE can actually pull include information...
# this should be used with the `powerpc-eabi-cmake` script included in:
# ${DEVKITPRO}/libogc2/gamecube/bin/ or ${DEVKITPRO}/libogc2/wii/bin/
# depending on your build target

# I would recommend (for convenience) making a symlink to them as:
# 'gc-cmake' and 'wii-cmake', and placing them in your path somewhere

cmake_minimum_required(VERSION 3.21)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(GTS LANGUAGES C)
set(PROJECT_EXPORT_NAME ${PROJECT_NAME}_${OGC_CONSOLE})

# these probably dont work
option(GDB_DEBUGGING "Remote GDB debugging" OFF)
option(LOGGING "Debug log" OFF)
option(BENCHMARK "Basic performamce benchmark" OFF)

# compiles textures specified in a .scf file provided
# basically copy-pasted from dkp-embedded-binary.cmake
function(dkp_add_embedded_textures target)
	if (NOT ${ARGC} EQUAL 2)
		message(FATAL_ERROR "dkp_add_embedded_textures: must provide one input file")
	endif ()

	if (NOT GXTEXCONV_EXE)
		message(FATAL_ERROR "Could not find gxtexconv: try reinstalling general-tools")
	endif ()

	set(genfolder "${CMAKE_CURRENT_BINARY_DIR}/.dkp-generated/${target}")

	set(intermediates "")
	foreach (inname IN LISTS ARGN)
		dkp_resolve_file(infiles "${inname}" MULTI)
		foreach(infile IN LISTS infiles)
			get_filename_component(basename "${infile}" NAME)
			string(REPLACE ".scf" "" basename "${basename}")

			if (TARGET "${inname}")
				set(indeps ${inname} ${infile})
			else()
				set(indeps ${infile})
			endif()

			add_custom_command(
					OUTPUT "${genfolder}/${basename}.tpl"
					COMMAND ${CMAKE_COMMAND} -E make_directory "${genfolder}"
					COMMAND ${GXTEXCONV_EXE} -s "${infile}" -d "${genfolder}/${basename}.d" -o "${genfolder}/${basename}.tpl"
					COMMAND ${DKP_BIN2S} -a ${DKP_BIN2S_ALIGNMENT} -H "${genfolder}/${basename}_tpl.h" "${genfolder}/${basename}.tpl" > "${genfolder}/${basename}_tpl.S"
					DEPENDS ${indeps}
					COMMENT "Converting textures as specified in ${inname}"
			)

			list(APPEND intermediates "${genfolder}/${basename}.tpl")
		endforeach()
	endforeach()
	add_library(${target} OBJECT ${intermediates})
	target_include_directories(${target} INTERFACE "${genfolder}")

endfunction()

# all .c files to be compiled
add_executable(
        # output name?
        ${PROJECT_EXPORT_NAME}

		source/main.c
		source/menu.c
		source/waveform.c

		# util sources
		source/util/args.c
		source/util/datetime.c
		source/util/file.c
		source/util/gx.c
		source/util/logging.c
		source/util/polling.c
		source/util/print.c
		source/util/stickmap.c
        source/util/arena.c

		# submenus
		source/submenu/continuous.c
		source/submenu/controllertest.c
		source/submenu/coordinateviewer.c
		source/submenu/errordisplay.c
		source/submenu/gate.c
		source/submenu/oscilloscope.c
		source/submenu/plot2d.c
		source/submenu/plotbutton.c
		source/submenu/trigger.c
)

# this does something weird either with the stack or include order
# coordview_json fails do decode on startup...
if(GDB_DEBUGGING)
	message("Enabling GDB Debugging")
	add_definitions(-DDEBUGGDB)
	target_link_libraries(${PROJECT_EXPORT_NAME} db)
endif()

if(LOGGING)
	message("Enabling output debug log")
	add_definitions(-DDEBUGLOG)
endif()

if (BENCHMARK)
	message("Enabling basic performance metrics")
	add_definitions(-DBENCH)
endif()

include_directories(
		# base include folder
		${PROJECT_SOURCE_DIR}/include
		# generated textures folder
		${CMAKE_CURRENT_BINARY_DIR}/textures
		# portlibs
		${DEVKITPRO}/portlibs/${OGC_CONSOLE}/include
		${DEVKITPRO}/portlibs/ppc/include
)

target_link_libraries(${PROJECT_EXPORT_NAME} jansson fat)

# all warnings...
target_compile_options(${PROJECT_EXPORT_NAME} PRIVATE -Wall)

# link textures if it exists
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/textures/textures.scf)
	add_dependencies(${PROJECT_EXPORT_NAME} texconv)
	dkp_add_embedded_textures(texconv ${CMAKE_CURRENT_SOURCE_DIR}/textures/textures.scf)
	get_target_property(TEXCONV_PATH texconv INTERFACE_INCLUDE_DIRECTORIES)
	target_include_directories(${PROJECT_EXPORT_NAME} PRIVATE ${TEXCONV_PATH})
	target_link_libraries(${PROJECT_EXPORT_NAME}
			${TEXCONV_PATH}/textures_tpl.S
	)
endif()

# compile json data
dkp_add_embedded_binary_library(BUILTIN_JSON
		${CMAKE_CURRENT_SOURCE_DIR}/data/plot2d_stickmaps.json
		${CMAKE_CURRENT_SOURCE_DIR}/data/coordview_stickmaps.json
)

add_dependencies(${PROJECT_EXPORT_NAME} BUILTIN_JSON)

# get directory out from target
# no idea how this should be done properly...
get_target_property(JSON_INCLUDES BUILTIN_JSON INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(${PROJECT_EXPORT_NAME} PRIVATE ${JSON_INCLUDES})
target_link_libraries(${PROJECT_EXPORT_NAME}
		# why does the order matter on these ???
		# adding a newline to coordview seems to fix it ??
		# there's a reason this is considered unsupported
		${JSON_INCLUDES}/coordview_stickmaps_json.S
		${JSON_INCLUDES}/plot2d_stickmaps_json.S
)

# add git commit id
execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE COMMIT_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_compile_definitions(${PROJECT_EXPORT_NAME} PRIVATE
        "-DCOMMIT_ID=\"${COMMIT_ID}\""
)

# add build date
execute_process(
        COMMAND date -u +%F_%H:%M:%S
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE BUILD_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_compile_definitions(${PROJECT_EXPORT_NAME} PRIVATE
        "-DBUILD_DATE=\"${BUILD_DATE}\""
)

# specify version string if found
if(NOT "$ENV{VERSION_STR}" STREQUAL "")
    target_compile_definitions(${PROJECT_EXPORT_NAME} PRIVATE
            "-DVERSION_NUMBER=\"$ENV{VERSION_STR}\""
    )
endif()

# build binary
ogc_create_dol(${PROJECT_EXPORT_NAME})
