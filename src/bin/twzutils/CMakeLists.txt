cmake_minimum_required(VERSION 3.14)

project(twzutils VERSION 0.1 DESCRIPTION "Twizzler Utility Programs")

add_executable(bench bench.c)
install(TARGETS bench DESTINATION bin)

add_executable(init_bootstrap init_bootstrap.c)

set_property(TARGET init_bootstrap PROPERTY LINK_LIBRARIES)
target_link_libraries(init_bootstrap twz_static)
target_link_libraries(init_bootstrap twix_static)

install(TARGETS init_bootstrap DESTINATION bin)
set_target_properties(init_bootstrap PROPERTIES LINK_FLAGS "-static")
set_target_properties(init_bootstrap PROPERTIES LINK_FLAGS "-static")


add_executable(login login.c)
install(TARGETS login DESTINATION bin)

add_executable(lscpu lscpu.c)
install(TARGETS lscpu DESTINATION bin)

add_executable(lsmem lsmem.c)
install(TARGETS lsmem DESTINATION bin)

add_executable(kls kls.c)
install(TARGETS kls DESTINATION bin)

add_executable(ostat ostat.c)
install(TARGETS ostat DESTINATION bin)

