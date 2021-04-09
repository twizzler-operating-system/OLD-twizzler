cmake_minimum_required(VERSION 3.14)

project(pager VERSION 0.1 DESCRIPTION "Twizzler pager")

add_executable(pager pager.cpp)
install(TARGETS pager DESTINATION bin)
