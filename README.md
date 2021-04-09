# README #

Twizzler is a research operating system designed to explore novel programming models for new memory
hierarchy designs. In particular, we are focused on the opportunities presented by byte-addressable
non-volatile memory technologies. Twizzler provides a data-centric programming environment that
allows programmers to operate on persistent data like it's memory -- because it is!

This repo contains source code for the kernel and userspace, along with a build system that
bootstraps a Twizzler userspace (including porting existing POSIX software). You can write code for
it and play around! We're not quite production ready, but we're getting there! :)

See https://twizzler.io/about.html for an overview of our goals.

Building
--------

See docs/BUILD.md for instructions. 

Writing some test code
----------------------

See src/playground/README.md. For an example of some of the Twizzler API, see src/playground/example.c

Documentation
-------------

Instructions coming soon!

