cmake_minimum_required(VERSION 3.14)

set(SYSROOT_DIR ${CMAKE_BINARY_DIR}/sysroot)
set(TOOLCHAIN_FILE ${CMAKE_BINARY_DIR}/cmake.toolchain)

project(Twizzler VERSION 0.1 DESCRIPTION "Twizzler Operating System")

set(CMAKE_INSTALL_PREFIX ${SYSROOT_DIR}/usr)

get_filename_component(TOOLCHAIN_BIN_DIR ${CMAKE_C_COMPILER} DIRECTORY)
get_filename_component(TOOLCHAIN_DIR ${TOOLCHAIN_BIN_DIR}/.. ABSOLUTE)

if(NOT EXISTS ${TOOLCHAIN_DIR}/bin/clang OR NOT EXISTS ${TOOLCHAIN_FILE}) 
	message(FATAL_ERROR "No valid toolchain found! Please see docs/BUILD.md for instructions on properly building Twizzler.")
endif()

include(ExternalProject)

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


ExternalProject_Add(
	"musl"
	CONFIGURE_COMMAND "${CMAKE_SOURCE_DIR}/src/lib/musl/configure"
	"CC=${TOOLCHAIN_DIR}/bin/clang"
	"LD=${TOOLCHAIN_DIR}/bin/clang"
	"ARCH=x86_64"
	"--host=x86_64-pc-twizzler-musl"
	"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -Wno-ignored-optimization-argument -Wno-unused-command-line-argument"
	"LDFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -Wno-unused-command-line-argument"
	"--prefix=/usr"
	"--syslibdir=/lib"
	"--enable-debug"
	"--enable-optimize"
	BUILD_COMMAND "make" "-s"
		"-j" "${BUILD_CORES}"
	INSTALL_COMMAND "make" "-s" "install" "DESTDIR=${SYSROOT_DIR}"
		"-j" "${BUILD_CORES}"
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/lib/musl"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	LOG_CONFIGURE ON
	BUILD_ALWAYS ${REBUILD_LIBC} #TODO make option
)

add_compile_options (-fcolor-diagnostics)
set(BUILD_SHARED_LIBS ON)
add_subdirectory(src)

