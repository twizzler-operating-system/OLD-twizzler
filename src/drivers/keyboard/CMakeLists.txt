cmake_minimum_required(VERSION 3.14)

project(drv_keyboard VERSION 0.1 DESCRIPTION "Twizzler keyboard driver")

add_executable(keyboard keyboard.c)
install(TARGETS keyboard DESTINATION bin)


