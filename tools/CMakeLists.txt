cmake_minimum_required(VERSION 3.14)
project(TwizzlerTools VERSION 0.1 DESCRIPTION "Twizzler Toolchain")

include(ExternalProject)

set(SYSROOT_DIR ${CMAKE_BINARY_DIR}/../sysroot)
set(TOOLCHAIN_DIR ${CMAKE_BINARY_DIR}/toolchain)
set(TOOLCHAIN_FILE ${CMAKE_BINARY_DIR}/../cmake.toolchain)

option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." FALSE)

include(ProcessorCount)
ProcessorCount(BUILD_CORES)
if(BUILD_CORES EQUAL 0)
	set(BUILD_CORES 1)
else()
	message(STATUS "Enabling parallel build for external projects with ${BUILD_CORES} cores")
endif()

if(NOT DEFINED TWZ_TARGET)
	message(FATAL_ERROR "Must specify target with TWZ_TARGET. Can be one of: [x86_64]")
endif()

function(compute_triple INPUT RESULT_NAME)
	# FIXME: derive target triple correctly
	set(${RESULT_NAME} "${${INPUT}}-pc-twizzler-musl" PARENT_SCOPE)
endfunction()

if(DEFINED TWIZZLER_TRIPLE)
	compute_triple(TWZ_TARGET tmp_triple)
	if(NOT (tmp_triple STREQUAL TWIZZLER_TRIPLE))
		message(FATAL_ERROR "Cannot change target of build directory. Please create a new build directory if you wish to specify a new target.")
	endif()
else()
	compute_triple(TWZ_TARGET tmp_triple)
	set(TWIZZLER_TRIPLE "${tmp_triple}" CACHE INTERNAL "Internal Twizzler triple")
	set(MAYBE_REBUILD_TOOLCHAIN ON)
endif()

string(REPLACE "-" ";" TRIPLE_LIST ${TWIZZLER_TRIPLE})

list(GET TRIPLE_LIST 0 TWIZZLER_PROCESSOR)
list(GET TRIPLE_LIST 1 TWIZZLER_MACHINE)
string(REPLACE "_" "-" TWIZZLER_PROCESSOR_HY ${TWIZZLER_PROCESSOR})

execute_process(
	COMMAND
	  "sed"
	  "-e" "s|@processor@|${TWIZZLER_PROCESSOR_HY}|g"
	  "-e" "s|@sysroot@|${SYSROOT_DIR}|g"
	  "-e" "s|@toolchain@|${TOOLCHAIN_DIR}|g"
	  "-e" "s|@target@|${TWIZZLER_TRIPLE}|g"
	  "-e" "s|@twztarget@|${TWZ_TARGET}|g"
	  "${CMAKE_SOURCE_DIR}/../cmake/cmake.toolchain.in"
	OUTPUT_FILE
	  "${TOOLCHAIN_FILE}"
)

include(../cmake/toolchain.cmake)
