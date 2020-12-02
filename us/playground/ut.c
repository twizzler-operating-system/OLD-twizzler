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

#include <sys/wait.h>

int main()
{
	fprintf(stderr, "Hello, World!\n");

	int pid;
	if(!(pid = fork())) {
		fprintf(stderr, "fork! child\n");
		for(long i = 0; i < 100000; i++) {
			__syscall6(0, 0, 0, 0, 0, 0, 0);
		}
		fprintf(stderr, "child exit\n");
		exit(123);
	}
	fprintf(stderr, "fork! parent: %d\n", pid);

	kill(pid, SIGSTOP);
	int status;
	int res;
	while((res = wait(&status)) <= 0) {
		fprintf(stderr, "wait returned %d %d\n", res, status);
	}

	fprintf(stderr, "wait returned %d %d\n", res, status);
	fprintf(stderr, "  %d %d\n", WIFEXITED(status), WEXITSTATUS(status));
	fprintf(stderr, "  %d %d\n", WIFSIGNALED(status), WTERMSIG(status));
	fprintf(stderr, "  %d %d\n", WIFSTOPPED(status), WSTOPSIG(status));
	fprintf(stderr, "  %d\n", WIFCONTINUED(status));

	fprintf(stderr, "KILL CONT\n");
	kill(pid, SIGCONT);
	while((res = wait(&status)) <= 0) {
		fprintf(stderr, "wait returned %d %d\n", res, status);
	}

	fprintf(stderr, "wait returned %d %d\n", res, status);
	fprintf(stderr, "  %d %d\n", WIFEXITED(status), WEXITSTATUS(status));
	fprintf(stderr, "  %d %d\n", WIFSIGNALED(status), WTERMSIG(status));
	fprintf(stderr, "  %d %d\n", WIFSTOPPED(status), WSTOPSIG(status));
	fprintf(stderr, "  %d\n", WIFCONTINUED(status));

	return 0;
}
