#include <stdio.h>

#include <twz/gate.h>

#include <twix/twix.h>

#include <twz/io.h>
#include <twz/thread.h>

#include <twz/security.h>

#include <err.h>
#include <signal.h>
#include <twz/queue.h>
#include <unistd.h>

int main()
{
	fprintf(stderr, "Hello, World!\n");

	int pid;
	if(!(pid = fork())) {
		for(;;) {
			fprintf(stderr, "fork! child\n");
			for(long i = 0; i < 10000; i++) {
				__syscall6(0, 0, 0, 0, 0, 0, 0);
			}
		}
		exit(0);
	}
	fprintf(stderr, "fork! parent: %d\n", pid);

	for(long i = 0; i < 1000000; i++) {
		__syscall6(0, 0, 0, 0, 0, 0, 0);
	}
	fprintf(stderr, "killing %d\n", pid);
	int r = kill(pid, 9);
	if(r == -1)
		err(1, "kill");

	return 0;
}
