cmake_minimum_required(VERSION 3.14)

project(twzplayground VERSION 0.1 DESCRIPTION "Twizzler Testing")

add_executable(example example.c)
install(TARGETS example DESTINATION bin)

add_executable(netapi netapi.c)
install(TARGETS netapi DESTINATION bin)

target_link_libraries(netapi twznet)

add_executable(net net.cpp)
install(TARGETS net DESTINATION bin)


