cmake_minimum_required(VERSION 3.14)

project(twzdevmgr VERSION 0.1 DESCRIPTION "Twizzler Device Manager")

add_executable(twzdevmgr pcie.c twzdev.c)
install(TARGETS twzdevmgr DESTINATION bin)
