cmake_minimum_required(VERSION 3.14)

project(drv_e1000 VERSION 0.1 DESCRIPTION "Twizzler e1000 driver")

add_executable(e1000 e1000.cpp)
install(TARGETS e1000 DESTINATION bin)


