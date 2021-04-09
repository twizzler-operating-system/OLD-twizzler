cmake_minimum_required(VERSION 3.14)

project(twznet VERSION 0.1 DESCRIPTION "Twizzler Networking Library")

add_library(twznet buf.c netaddr.c net.c)

set_target_properties(twznet PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(twznet PROPERTIES SOVERSION 0)
target_compile_options(twznet PRIVATE "-fPIC")
target_link_options(twznet PRIVATE "-Wl,-z,notext")

target_include_directories(twznet
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include/nstack>
	PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include-priv>
	)

include(GNUInstallDirs)

install(TARGETS twznet
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(twznet.pc.in twznet.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/twznet.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)


install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/nstack DESTINATION include)
