cmake_minimum_required(VERSION 3.14)

project(init VERSION 0.1 DESCRIPTION "Twizzler init program")

add_executable(init main.c)
install(TARGETS init DESTINATION bin)

target_link_libraries(init twzsec)
target_link_libraries(init libtommath.a)
target_link_libraries(init libtomcrypt.a)
