# README #

Twizzler is a research operating system designed to explore novel programming models for new memory
hierarchy designs. In particular, we are focused on the opportunities presented by byte-addressable
non-volatile memory technologies. Twizzler provides a data-centric programming environment that
lets programmers to work with persistent data like it's memory -- because persistent data _is_ memory.

This repo contains source code for the kernel and userspace, along with a build system that
bootstraps a Twizzler userspace (including porting existing POSIX software). You can write code for
it and play around! We're not quite production ready, but we're getting there! :)

See https://twizzler.io/about.html for an overview of our goals.

A Tour of the Repo
------------------

path/to/twizzler
    cmake -- files for cmake build system
	docs -- documentation files
	ports -- files for patching and building ported software
	src
	  bin -- sources for twizzler programs
	  boot -- files for booting twizzler
	  drivers -- drivers for twizzler
	  kernel -- twizzler kernel sources
	    arch -- architecture-specific code
		core -- main kernel source, non-system specific
		include -- kernel-specific include files
		lib -- kernel library files
		machine -- platform-specific code
	  lib -- sources for twizzler libraries
	  playground -- testing area, write your hello world program in here :)
	  share -- files that get installed in /usr/share in the sysroot
	tools
	  libtom* -- libraries for both kernel and userspace for crypto and math
	  llvm-project -- LLVM and clang sources, used to build the toolchain
	  utils -- utilities for compiling twizzler stuff on unix

Building
--------

See docs/BUILD.md for instructions for building the OS. To build the documentation, run the
following commands:

    doxygen docs/Doxygen.user
	doxygen docs/Doxygen.kernel

This will build the user and kernel docs, placing them in docs-gen-user and docs-gen-kernel.

Writing some test code
----------------------

See src/playground/README.md. For an example of some of the Twizzler API, see src/playground/example.c

Documentation
-------------

Instructions coming soon!

