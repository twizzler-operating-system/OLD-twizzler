cmake_minimum_required(VERSION 3.14)

project(unix VERSION 0.1 DESCRIPTION "Twizzler Unix Emulator")

add_executable(unix clone.cpp cmd.cpp dir.cpp files.cpp mmap.cpp state.cpp unix.cpp async.cpp)

target_compile_options(unix PRIVATE "-std=c++17")

target_link_libraries(unix twzsec)
target_link_libraries(unix libtommath.a)
target_link_libraries(unix libtomcrypt.a)

install(TARGETS unix DESTINATION bin)
