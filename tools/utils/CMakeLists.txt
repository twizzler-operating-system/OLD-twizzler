cmake_minimum_required(VERSION 3.14)

project(twzunixutils VERSION 0.1 DESCRIPTION "Twizzler Unix Bootstrapping Utility Programs")

find_library(libtommath tommath)
find_library(libtomcrypt tomcrypt)

include_directories(${CMAKE_SOURCE_DIR}/../../src/lib/twz/include)

add_executable(appendobj appendobj.c)
install(TARGETS appendobj DESTINATION bin)

add_executable(file2obj file2obj.c blake2.c)
install(TARGETS file2obj DESTINATION bin)

add_executable(hier hier.c)
install(TARGETS hier DESTINATION bin)

add_executable(objstat objstat.c blake2.c)
install(TARGETS objstat DESTINATION bin)

add_executable(user user.c)
install(TARGETS user DESTINATION bin)


