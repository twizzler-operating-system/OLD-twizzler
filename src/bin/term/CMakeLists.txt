cmake_minimum_required(VERSION 3.14)

project(term VERSION 0.1 DESCRIPTION "Twizzler terminal emulator")

add_executable(term kbd.c term.c)
install(TARGETS term DESTINATION bin)
