cmake_minimum_required(VERSION 3.14)

project(twzdev VERSION 0.1 DESCRIPTION "Twizzler Device Manager")

add_executable(twzdev pcie.c twzdev.c)
install(TARGETS twzdev DESTINATION bin)
