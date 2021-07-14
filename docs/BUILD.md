So you want to build Twizzler {#building}
=============================

This is an operating system kernel and userspace, so the build is a little involved! This document
assumes you are trying to bootstrap a Twizzler environment on a unix OS.

Build Environment
-----------------

We currently support building primarily on Ubuntu 20.04 LTS. While building on other (unix) systems
should work, we don't test other environments. To setup a sufficient build system on Ubuntu 20.04,
you can use the following apt commands to install the required programs:

```
apt-get install build-essential cmake xorriso mtools libtommath-dev libtomcrypt-dev ninja-build clang
apt-get install python3.8 python3-distutils gcc-multilib zlib1g zlib1g-dev llvm llvm-dev grub2-common
```

Additionally, the build system expects python version 3 to be the default python executable on the
system. On Ubuntu, this can be done by running:

```
update-alternatives --install /usr/bin/python python /usr/bin/python3 10
```

If you want to build the documentation, you need to install the following:
```
apt-get install graphviz global doxygen
```

To run Twizzler, you have two options. In both cases, the machine you run on must be an Intel x86_64
machine (that is relatively recent). The two options are:
1. Emulate. The script `tools/start_qemu.sh` starts up QEMU with the necessary settings.
2. Real hardware. The build system creates a boot.iso file that can be used to boot Twizzler on
   machines that can support booting an ISO image (either a disk or a USB flash drive).

A Quick Tour
------------
This repo contains code to build the Twizzler kernel and basic userspace. It also contains a porting
system that will automatically compile some third-party software (bash, busybox, etc).

The following components will be build:
 * The toolchain: a toolchain is a compiler and linker (and related tools) that are used to build
   the source. We are using LLVM and Clang (and LLVM's linker) as our toolchain. However, we still
   need to teach clang and ld.lld how to compile and link code for Twizzler. For this reason, we're
   using a forked LLVM repo that contains the necessary modifications. Additionally, we compile the
   rust compiler to support Twizzler. This requires llvm and clang, and so is built as part of the
   toolchain.
 * The kernel: the Twizzler kernel contains code found in the src/kernel subdirectory.
 * The utilities: Found in tools/utils/, these programs are used to bootstrap a Twizzler environment. They
   largely consist of programs designed to turn unix concepts (like a file) into Twizzler concepts
   (like objects).
 * The userspace (under src/)
   * musl: This is a C library designed for Linux. Twizzler presents a system-call-level compatible
     Linux interface for POSIX programs (see Twix, below).
   * lib/twz: The standard Twizzler library. Contains the libos, default fault handing logic, and
	 functions to work with Twizzler concepts.
   * lib/twix: emulation for unix (presents a Linux syscall interface).
   * bin/, drivers/: a collection of Twizzler utilities (login, init, etc) and drivers.
 * ports (under ports): a set of software projects ported to Twizzler.

The three "core" userspace libraries (libtwz, twix, and musl) are typically all linked into a given
C program automatically if you use the proper toolchain.

The build system largely targets a "sysroot" (system root) directory. This is a directory that the
toolchain considers to be the "root" of the FS that the new OS will consider '/'. This is a slight
lie for Twizzler, because of the way it handles objects and naming, but it's close enough.

Once things are installed into the sysroot, we can build a ramdisk image and a bootable ISO image.
The bootable ISO contains the ramdisk and the kernel, and can be booted on a computer (or an
emulator).

Building Twizzer the First Time
-------------------------------

Before you begin! Make sure your git submodules are updated:

```
   git submodule update --init --recursive
```

*Part 1 -- The Toolchain*

First, we need to make a build directory in which all (*cough* er, most) of the build artifacts will
end up:
```   
   cd path/to/twizzler/source
   mkdir build && cd build
```
In here, we will be building three things. The toolchain, the system, and the ported software. Make
some subdirectories for the toolchain and the ports:
```
   mkdir tools ports && cd tools
```
Next, configure the toolchain. The main option, here, is TWZ_TARGET, which must be set to x86_64:
```
   cmake ../../tools -G Ninja -DTWZ_TARGET=x86_64
```
Finally, build the toolchain (this step takes a long time, as it must compile all of LLVM):
```
	ninja
```
No install step is needed -- this build system is largely a wrapped around a bunch of subprojects
that all get installed as part of the build. Once you've done this, your build tree should be:

* build/
  * ports/ (empty)
  * tools/ (you are here)
    * toolchain/ (the install location of the toolchain. The clang binary should be in bin/ in here)
  * cmake.toolchain (a file describing the toolchain)
  * sysroot/ (the system root, with some initial stuff installed)

*Part 2 -- Twizzler*

First, cd back up to the main build directory:
```
   cd ..
```
Okay, now we need to configure the system. We need to pass the same TWZ_TARGET as before, but also
two new options: CMAKE_BUILD_TYPE can be set to either "Debug" or "Release", and
CMAKE_TOOLCHAIN_FILE must be set to the cmake.toolchain file in this directory:
```
   cmake .. -G Ninja -DTWZ_TARGET=x86_64 -DCMAKE_TOOLCHAIN_FILE=cmake.toolchain -DCMAKE_BUILD_TYPE=Release
```
Next, build the Twizzler system (phase 1)
```
   ninja
```
Note that I said "phase 1". This is because part of what we had to do in the toolchain part above
is bootstrap the system root with some libc headers. There's this circular dependency on libraries
here: libc depends on libtwix and libtwz, while each of those depend on the other two. As a result,
we need to install the "real" libraries as part of this first build step, and then rebuild software
to ensure that it gets the "real" version of the libraries:
```
   ninja install
   ninja
```
Yeah, it's annoying. But you only need to do this once. With that done, we can move on to ports.

*Part 3 -- Ports*

First, cd into the ports directory:
```
   cd ports
```
Configure them. This requires similar options as part 2:
```
   cmake ../../ports -G Ninja -DTWZ_TARGET=x86_64 -DCMAKE_TOOLCHAIN_FILE=../cmake.toolchain -DCMAKE_BUILD_TYPE=Release
```
And build:
```
   ninja
```
Again, the ports get installed into sysroot as part of the build process.

*Epilogue -- Making an ISO*

Now that we've built the sysroot, we can make an ISO image that will actually boot. Run this in
build/:
```
   ninja bootiso
```
And if that succeeds, you can go ahead and try it out in Qemu:
```
   ../tools/start_qemu.sh
```
And there ya go! Hit enter at the login prompt for a default login.

Building Twizzler Again
-----------------------

Most of the steps above can be skipped on repeat builds. For internal software, the system will
track dependencies fairly well. To rebuild Twizzler, typically one would run:
```
   ninja
```
The 'bootiso' target will build the ISO image again.

Note that external software (the stuff ported via the ports system) won't be recompiled! Twizzler
does use dynamic linking, but if there's an ABI change the ports will break. If bash stops working,
try giving it the ol' recompile.

Testing Multiple Instances of Twizzler (for networking)
-------------------------------------------------------
By default, the qemu start script will start 1 instance of Twizzler. You can also, optionally, specify
an "INSTANCES" variable on the command line to start more than one instance of Twizzler:
```
QEMU_FLAGS='-display none' INSTANCES=<instance command line> ../tools/start_qemu.sh
```
where the <instance command line> is a space-separated list of instance commands that look like this:
<net-type>,<arg>,<arg>...

<net-type> is either 's' or 't' (for "socket" or "tap" respectively).

For socket net-types, the first arg is "l" or "c" for "listener" or "client". The second arg is the port
number for the socket netdev. The third (optional) argument is the mac address for the card. The listener
has to be first.

For tap net-types, the first arg is the interface name to use (eg vport_twz1), and the second (optional) arg is the mac.

EXAMPLES:
to create two twizzler instances that connect to each other (via socket netdev): INSTANCES="s,l,1234,11:22:33:44:55:66 s,c,1234,22:33:44:55:66:77"
to create two twizzler instances that connect to tap interfaces: INSTANCES="t,vport_twz1,<mac> t,vport_twz2,<mac>"

IMPORTANT: These MUST have a serial unix socket ready and waiting before you start this command. To
do this, run:
```
	socat UNIX-LISTEN:twz_serial_<N>.sock,fork -,cfmakeraw
```
for each instance that will be created (replacing <N> with each instance ID). So if you want to
create two twizzler instances, you'll need to also have running in the background:
```
	socat UNIX-LISTEN:twz_serial_1.sock,fork -,cfmakeraw
	socat UNIX-LISTEN:twz_serial_2.sock,fork -,cfmakeraw
```
Each instance will write packet capture from the NIC to twz_packetdump_<instance #>.dat.
