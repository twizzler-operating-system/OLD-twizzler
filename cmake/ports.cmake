if(BUILD_PORTS)

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
	BUILD_ALWAYS OFF #${BUILD_PORTS}
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
	PATCH_COMMAND "cp" ${CMAKE_SOURCE_DIR}/ports/config.sub
	${CMAKE_SOURCE_DIR}/ports/bash/support/Makefile.in
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
	BUILD_ALWAYS OFF #${BUILD_PORTS}
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
	PATCH_COMMAND "cp" ${CMAKE_SOURCE_DIR}/ports/busybox/busybox-config
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
	BUILD_ALWAYS OFF#${BUILD_PORTS}
	)

add_custom_target(ports
	DEPENDS
	"curses"
	"bash"
	"busybox"
)

endif()
