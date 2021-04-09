cmake_minimum_required(VERSION 3.14)

project(logboi VERSION 0.1 DESCRIPTION "Twizzler logging program")

add_executable(logboi logboi.cpp)

target_include_directories(logboi
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
	PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include-priv>
	)

target_link_libraries(logboi twzsec)
target_link_libraries(logboi ${SYSROOT_DIR}/usr/lib/libtommath.a)
target_link_libraries(logboi ${SYSROOT_DIR}/usr/lib/libtomcrypt.a)

install(TARGETS logboi DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/logboi DESTINATION include)
