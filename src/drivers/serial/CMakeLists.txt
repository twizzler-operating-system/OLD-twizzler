cmake_minimum_required(VERSION 3.14)

project(drv_serial VERSION 0.1 DESCRIPTION "Twizzler serial driver")

add_executable(serial serial.c)
install(TARGETS serial DESTINATION bin)


