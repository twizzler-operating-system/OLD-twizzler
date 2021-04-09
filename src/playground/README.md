Creating a program to play around with Twizzler
-----------------------------------------------

1) create a C file in src/playground, eg. src/playground/foo.c

2) add to src/playground/CMakeLists.txt:

   add_executable(foo foo.c)
   install(TARGETS foo DESTINATION bin)

3) Start writing code in foo.c; this file will be automatically compiled into a program called 'foo'
and placed in usr/bin in the sysroot.

