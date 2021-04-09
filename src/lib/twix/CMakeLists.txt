cmake_minimum_required(VERSION 3.14)

project(twix VERSION 0.1 DESCRIPTION "Twizzler Unix Emulation Library")

# TODO: remove oa
if(BUILD_SHARED_LIBS)
add_library(twix access.c dir.c fd.c file.c linux.c process.c rand.c rw.c syscall.c thread.c
	time.c v2/fork.c v2/rw.c v2/twix.c)
set_target_properties(twix PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(twix PROPERTIES SOVERSION 0)
target_compile_options(twix PRIVATE "-fPIC")
target_link_options(twix PRIVATE "-Wl,-z,notext")

# FIXME should separate includes
target_include_directories(twix
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../twz/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include/twz>
	$<INSTALL_INTERFACE:include/twix>
	)
endif()

add_library(twix_static STATIC access.c dir.c fd.c file.c linux.c process.c rand.c rw.c syscall.c thread.c
	time.c v2/fork.c v2/rw.c v2/twix.c)
set_target_properties(twix_static PROPERTIES OUTPUT_NAME twix)

set_target_properties(twix_static PROPERTIES VERSION ${PROJECT_VERSION})

target_compile_options(twix_static PRIVATE "-fPIC" "-mclwb" "-mclflushopt" "-Wno-gnu-variable-sized-type-not-at-end")
target_link_options(twix_static PRIVATE "-Wl,-z,notext")

# FIXME should separate includes
target_include_directories(twix_static
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../twz/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include/twz>
	$<INSTALL_INTERFACE:include/twix>
	)

include(GNUInstallDirs)

if(BUILD_SHARED_LIBS)
install(TARGETS twix
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
install(TARGETS twix_static
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(twix.pc.in twix.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/twix.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/twix DESTINATION include)


