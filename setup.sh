#!/bin/bash

set -xe

sudo PROJECT=x86_64 make tools-prep
sudo PROJECT=x86_64 make allutils
sudo PROJECT=x86_64 ./us/ports/port.sh libbacktrace
sudo PROJECT=x86_64 ./us/ports/port.sh ncurses
sudo PROJECT=x86_64 ./us/ports/port.sh bash
sudo PROJECT=x86_64 ./us/ports/port.sh busybox
sudo PROJECT=x86_64 ./us/ports/port.sh tommath
sudo PROJECT=x86_64 ./us/ports/port.sh tomcrypt
