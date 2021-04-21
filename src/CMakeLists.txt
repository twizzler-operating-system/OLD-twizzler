cmake_minimum_required(VERSION 3.14)

project(Twizzler VERSION 0.1 DESCRIPTION "Twizzler OS")

#include(FetchContent)

#set(Rust_COMPILER ${TOOLCHAIN_DIR}/bin/rustc)
#set(Rust_CARGO_TARGET ${TWIZZLER_TRIPLE})
#set(Rust_ROOT ${TOOLCHAIN_DIR}/lib/rustlib/x86_64-pc-twizzler-musl)
#FetchContent_Declare(
#	Corrosion
#	GIT_REPOSITORY https://github.com/AndrewGaspar/corrosion.git
#	GIT_TAG origin/master # Optionally specify a version tag or branch here
#)

#FetchContent_MakeAvailable(Corrosion)

set(ENV{RUSTFLAGS} "--target=${TWIZZLER_TRIPLE} -C link-args=\"--sysroot ${SYSROOT_DIR}\" -C linker=${TOOLCHAIN_DIR}/bin/clang")

add_compile_options("-mclwb" "-mclflushopt")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	add_compile_options("-g" "-DDEBUG" "-Og" "-Werror")
	set(CARGO_BUILD_TYPE "debug")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	add_compile_options("-g" "-O3")
	set(CARGO_BUILD_TYPE "release")
else()
	message(FATAL_ERROR "Please specify CMAKE_BUILD_TYPE as either `Release' or `Debug'")
endif()

set(CMAKE_C_STANDARD "11")
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD "17")
set(CMAKE_CXX_EXTENSIONS ON)
add_compile_options("-Wno-gnu-variable-sized-type-not-at-end")
add_compile_options("-Wno-c99-designator")
add_compile_options("-Werror")

add_subdirectory(kernel)

add_subdirectory(lib/twz)
add_subdirectory(lib/twix)

include_directories(lib/twz/include)
include_directories(lib/twix/include)
include_directories(lib/net/include)

add_custom_command(
	OUTPUT ${SYSROOT_DIR}/usr/lib/libc.so
	DEPENDS twz_static twix_static musl
	COMMAND make -s -C ${CMAKE_BINARY_DIR}/musl-prefix/src/musl-build lib/libc_updated.so LIBTWIX_A=$<TARGET_LINKER_FILE:twix_static> LIBTWZ_A=$<TARGET_LINKER_FILE:twz_static>
	COMMAND install -C -D ${CMAKE_BINARY_DIR}/musl-prefix/src/musl-build/lib/libc_updated.so ${SYSROOT_DIR}/usr/lib/libc.so
)

add_custom_target(libc ALL
	DEPENDS ${SYSROOT_DIR}/usr/lib/libc.so
)

install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink /usr/lib/libc.so ${SYSROOT_DIR}/lib/ld64.so)")
install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E make_directory ${SYSROOT_DIR}/bin)")
install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink /usr/bin/bash ${SYSROOT_DIR}/bin/sh)")

add_subdirectory(lib/net)
add_subdirectory(lib/security)

link_libraries(twz)

add_subdirectory(bin/init)
#add_subdirectory(bin/logboi)
add_subdirectory(bin/network)
add_subdirectory(bin/pager)
add_subdirectory(bin/term)
add_subdirectory(bin/twzdev)
add_subdirectory(bin/twzutils)
add_subdirectory(bin/unix)

add_subdirectory(drivers/e1000)
add_subdirectory(drivers/keyboard)
add_subdirectory(drivers/nvme)
add_subdirectory(drivers/serial)

add_subdirectory(playground)


#"CARGO_TERM_VERBOSE=true"

add_rust_target(rust-test playground/rust-test)

add_rust_target(logboi-rs bin/logboi-rs)

add_custom_command(
	OUTPUT
	${SYSROOT_DIR}/bob.user
	${SYSROOT_DIR}/bob.kring
	${SYSROOT_DIR}/bob.sctx
	DEPENDS ${TOOLCHAIN_DIR}/bin/user
	COMMAND
	${TOOLCHAIN_DIR}/bin/user -u ${SYSROOT_DIR}/bob.user
	-r ${SYSROOT_DIR}/bob.kring bob)

add_custom_target(bob
	DEPENDS ${SYSROOT_DIR}/bob.user)

install(CODE "execute_process(COMMAND touch ${SYSROOT_DIR}/boot/stamp)")

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ramdisk.tar
	COMMAND python
	${CMAKE_SOURCE_DIR}/tools/gen_ram_disk.py ${SYSROOT_DIR} ${CMAKE_BINARY_DIR}
	${TOOLCHAIN_DIR}/bin/hier
	${TOOLCHAIN_DIR}/bin/file2obj
	${TOOLCHAIN_DIR}/bin/objstat
	${TOOLCHAIN_DIR}/bin/appendobj
	DEPENDS ${TOOLCHAIN_DIR}/bin/hier
	DEPENDS ${TOOLCHAIN_DIR}/bin/file2obj
	DEPENDS ${TOOLCHAIN_DIR}/bin/objstat
	DEPENDS ${TOOLCHAIN_DIR}/bin/appendobj
	OUTPUT ${SYSROOT_DIR}/boot/stamp_ DEPENDS ${SYSROOT_DIR}/bob.user DEPENDS install)

add_custom_target(ramdisk DEPENDS ${CMAKE_BINARY_DIR}/ramdisk.tar DEPENDS ${SYSROOT_DIR}/boot/stamp
	DEPENDS ${SYSROOT_DIR}/boot/stamp DEPENDS ${SYSROOT_DIR}/bob.sctx)


add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/boot.iso
	COMMAND
	mkdir -p ${CMAKE_BINARY_DIR}/bootdisk_add/boot/grub
	COMMAND
	cp ${CMAKE_SOURCE_DIR}/src/boot/grub/grub.cfg ${CMAKE_BINARY_DIR}/bootdisk_add/boot/grub
	COMMAND
	grub-mkrescue -o ${CMAKE_BINARY_DIR}/boot.iso $<TARGET_FILE:kernel>
	${CMAKE_BINARY_DIR}/ramdisk.tar ${CMAKE_BINARY_DIR}/bootdisk_add
	DEPENDS kernel
	DEPENDS ${CMAKE_BINARY_DIR}/ramdisk.tar
)

add_custom_target(bootdisk DEPENDS ${CMAKE_BINARY_DIR}/boot.iso DEPENDS
	${CMAKE_BINARY_DIR}/ramdisk.tar DEPENDS
	${CMAKE_SOURCE_DIR}/src/boot/grub/grub.cfg DEPENDS $<TARGET_FILE:kernel>)



