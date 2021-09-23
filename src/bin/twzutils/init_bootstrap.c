/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <unistd.h>

#include <twz/debug.h>

extern int __name_bootstrap(void);
int main()
{
	debug_printf("Bootstrapping naming system\n");
	if(__name_bootstrap() == -1) {
		debug_printf("Failed to bootstrap namer\n");
		abort();
	}
	unsetenv("BSNAME");
	setenv("TERM", "linux", 1);
	setenv("PATH", "/bin:/usr/bin", 1);
	setenv("LD_LIBRARY_PATH", "/lib:/usr/lib", 1);
	setenv("RUST_BACKTRACE", "full", 1);

	execlp("/usr/bin/init", "init", NULL);
	debug_printf("Failed to start init\n");
	return -1;
}
