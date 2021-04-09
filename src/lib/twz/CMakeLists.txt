cmake_minimum_required(VERSION 3.14)

project(twz VERSION 1.0 DESCRIPTION "Twizzler Standard Library")

# TODO: remove oa
add_library(twz_static STATIC alloc.c bstream.c driver.c event.c fault.c hier.c io.c kso.c libtwz.c mutex.c name.c object.c pty.c queue.c seccall.c thread.c view.c)
if(BUILD_SHARED_LIBS)
add_library(twz SHARED alloc.c bstream.c driver.c event.c fault.c hier.c io.c kso.c libtwz.c mutex.c name.c object.c pty.c queue.c seccall.c thread.c view.c)
endif()

set_target_properties(twz_static PROPERTIES OUTPUT_NAME twz)
set_target_properties(twz_static PROPERTIES VERSION ${PROJECT_VERSION})
target_compile_options(twz_static PRIVATE "-include" "libtwz.h" "-fPIC" "-mclwb" "-mclflushopt")
target_link_options(twz_static PRIVATE "-Wl,-z,notext")
if(BUILD_SHARED_LIBS)
set_target_properties(twz PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(twz PROPERTIES SOVERSION 1)
target_compile_options(twz PRIVATE "-include" "libtwz.h" "-fPIC")
target_link_options(twz PRIVATE "-Wl,-z,notext")
endif()


# FIXME should separate includes
if(BUILD_SHARED_LIBS)
target_include_directories(twz
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include/twz>
	PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include-priv>
	)
endif()

target_include_directories(twz_static
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include/twz>
	PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include-priv>
	)

include(GNUInstallDirs)

if(BUILD_SHARED_LIBS)
install(TARGETS twz
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

install(TARGETS twz_static
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(twz.pc.in twz.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/twz.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/twz DESTINATION include)


