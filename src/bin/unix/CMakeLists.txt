cmake_minimum_required(VERSION 3.14)

project(unix VERSION 0.1 DESCRIPTION "Twizzler Unix Emulator")

add_executable(unix clone.cpp cmd.cpp dir.cpp files.cpp mmap.cpp state.cpp unix.cpp)

target_compile_options(unix PRIVATE "-std=c++17")

find_library(tommath tommath)
find_library(tomcrypt tomcrypt)
target_link_libraries(unix twzsec)
target_link_libraries(unix ${tommath})
target_link_libraries(unix ${tomcrypt})

install(TARGETS unix DESTINATION bin)
