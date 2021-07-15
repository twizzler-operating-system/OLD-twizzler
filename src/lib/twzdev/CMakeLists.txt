cmake_minimum_required(VERSION 3.14)

project(twzdev VERSION 0.1 DESCRIPTION "Twizzler Device Management Library")

add_library(twzdev dev.c handler.c)

set_target_properties(twzdev PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(twzdev PROPERTIES SOVERSION 0)
target_compile_options(twzdev PRIVATE "-fPIC")
target_link_options(twzdev PRIVATE "-Wl,-z,notext")

target_include_directories(twzdev
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
	PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include-priv>
	)

include(GNUInstallDirs)

install(TARGETS twzdev
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(twzdev.pc.in twzdev.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/twzdev.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)


install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/twz/sys/dev DESTINATION include/twz/sys/dev)
