cmake_minimum_required(VERSION 3.14)

project(network VERSION 0.1 DESCRIPTION "Twizzler network manager")

add_executable(network arp.cpp char_ring_buffer.cpp client.cpp client_handling_testing.cpp
	common.cpp databuf.cpp encapsulate.cpp eth.cpp generic_ring_buffer.cpp interface.cpp ipv4.cpp
	network.cpp tcp_conn.cpp tcp.cpp twz.cpp twz_op.cpp udp_conn.cpp udp.cpp)
install(TARGETS network DESTINATION bin)

find_library(tommath tommath)
find_library(tomcrypt tomcrypt)
target_link_libraries(network twzsec)
target_link_libraries(network ${tommath})
target_link_libraries(network ${tomcrypt})

