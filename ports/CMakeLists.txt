cmake_minimum_required(VERSION 3.14)

set(SYSROOT_DIR ${CMAKE_BINARY_DIR}/../sysroot)
set(TOOLCHAIN_FILE ${CMAKE_BINARY_DIR}/../cmake.toolchain)

project(TwizzlerPorts VERSION 0.1 DESCRIPTION "Twizzler Ports")

set(CMAKE_INSTALL_PREFIX ${SYSROOT_DIR}/usr)

get_filename_component(TOOLCHAIN_BIN_DIR ${CMAKE_C_COMPILER} DIRECTORY)
get_filename_component(TOOLCHAIN_DIR ${TOOLCHAIN_BIN_DIR}/.. ABSOLUTE)

include(ExternalProject)


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
	"curses"
	GIT_REPOSITORY "https://github.com/sabotage-linux/netbsd-curses"
	CONFIGURE_COMMAND ""
	BUILD_COMMAND
	"make"
	"CC=${TOOLCHAIN_DIR}/bin/clang -target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR}"
	"HOSTCC=/usr/bin/cc"
	"LD=${TOOLCHAIN_DIR}/bin/clang"
	"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -O2 -g"
	"LDFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR}"
	"PREFIX=/usr"
	"-j" "${BUILD_CORES}"
	"install"
	"DESTDIR=${SYSROOT_DIR}"
	INSTALL_COMMAND ""
	SOURCE_DIR "${CMAKE_BINARY_DIR}/ports/curses"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	BUILD_IN_SOURCE ON
	BUILD_ALWAYS ON
	LOG_BUILD ON
	LOG_UPDATE ON
	LOG_CONFIGURE ON
	LOG_DOWNLOAD ON
	DOWNLOAD_NO_PROGRESS ON
	LOG_INSTALL ON
)

ExternalProject_Add(
	"bash"
	DEPENDS "curses"
	URL "https://mirrors.ocf.berkeley.edu/gnu/bash/bash-5.0.tar.gz"
	DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/ports
	SOURCE_DIR ${CMAKE_BINARY_DIR}/ports/bash-5.0
	PATCH_COMMAND "cp" ${CMAKE_SOURCE_DIR}/config.sub
	${CMAKE_SOURCE_DIR}/bash/support/Makefile.in
	${CMAKE_BINARY_DIR}/ports/bash-5.0/support/
	CONFIGURE_COMMAND
	"${CMAKE_BINARY_DIR}/ports/bash-5.0/configure"
	"--host=x86_64-pc-twizzler-musl"
	"CC=${TOOLCHAIN_DIR}/bin/clang -target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR}"
	"LD=${TOOLCHAIN_DIR}/bin/clang"
	"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -O2 -g -fPIC"
	"LDFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -Wl,-z,notext"
	"LIBS=-lterminfo"
	"LIBS_FOR_BUILD="
	"--without-bash-malloc"
	"--with-curses"
	"--disable-nls"
	"--prefix=/usr"
	BUILD_COMMAND
	"make" "-j" "${BUILD_CORES}"
	INSTALL_COMMAND
	"make" "-j" "${BUILD_CORES}" "DESTDIR=${SYSROOT_DIR}" "install"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	BUILD_IN_SOURCE ON
	BUILD_ALWAYS ON
	LOG_BUILD ON
	LOG_CONFIGURE ON
	LOG_DOWNLOAD ON
	DOWNLOAD_NO_PROGRESS ON
	LOG_INSTALL ON
)

ExternalProject_Add(
	"busybox"
	DEPENDS "curses"
	URL "https://busybox.net/downloads/busybox-1.31.1.tar.bz2"
	DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/ports
	SOURCE_DIR ${CMAKE_BINARY_DIR}/ports/busybox-1.31.1
	PATCH_COMMAND "cp" ${CMAKE_SOURCE_DIR}/busybox/busybox-config
	${CMAKE_BINARY_DIR}/ports/busybox-1.31.1/.config
	BUILD_IN_SOURCE ON
	CONFIGURE_COMMAND ""
	BUILD_COMMAND "make" "V=1"
	"CC=${TOOLCHAIN_DIR}/bin/clang -target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR}"
	"LD=${TOOLCHAIN_DIR}/bin/clang"
	"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -O2 -g -fPIC"
	"LDFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -Wl,-z,notext"
	"install"
	INSTALL_COMMAND
	cp -a ${CMAKE_BINARY_DIR}/ports/busybox-1.31.1/_install/bin ${SYSROOT_DIR}/usr/
	LOG_BUILD ON
	LOG_CONFIGURE ON
	LOG_DOWNLOAD ON
	DOWNLOAD_NO_PROGRESS ON
	LOG_INSTALL ON
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	BUILD_ALWAYS ON
	)

add_custom_target(ports
	DEPENDS
	"curses"
	"bash"
	"busybox"
)


