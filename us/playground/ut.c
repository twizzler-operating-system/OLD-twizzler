#include <stdio.h>

#include <twz/gate.h>

#include <twix/twix.h>

#include <twz/io.h>
#include <twz/thread.h>

#include <twz/security.h>

#include <twz/queue.h>

int main()
{
	fprintf(stderr, "Hello, World!\n");

	int pid;
	if(!(pid = fork())) {
		fprintf(stderr, "fork! child\n");
		exit(0);
	}
	fprintf(stderr, "fork! parent: %d\n", pid);

	return 0;
}
