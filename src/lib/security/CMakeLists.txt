cmake_minimum_required(VERSION 3.14)

project(twzsec VERSION 0.1 DESCRIPTION "Twizzler Security Library")

add_library(twzsec sctx.c)

set_target_properties(twzsec PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(twzsec PROPERTIES SOVERSION 0)
target_compile_options(twzsec PRIVATE "-fPIC")
target_link_options(twzsec PRIVATE "-Wl,-z,notext")

target_include_directories(twzsec
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
	PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include-priv>
	)

include(GNUInstallDirs)

install(TARGETS twzsec
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(twzsec.pc.in twzsec.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/twzsec.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

