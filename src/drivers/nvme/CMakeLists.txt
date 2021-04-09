cmake_minimum_required(VERSION 3.14)

project(drv_nvme VERSION 0.1 DESCRIPTION "Twizzler nvme driver")

add_executable(nvme nvme.cpp)
install(TARGETS nvme DESTINATION bin)


