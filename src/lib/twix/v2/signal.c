#include "sys.h"
#include "v2.h"
#include <signal.h>
#include <twz/sys/fault.h>
#include <twz/sys/thread.h>
#define SA_DFL                                                                                     \
	(struct k_sigaction)                                                                           \
	{                                                                                              \
		.handler = SIG_DFL                                                                         \
	}
#define NUM_SIG 64

struct k_sigaction {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)(void);
	unsigned mask[2];
};

struct k_sigaction _signal_actions[NUM_SIG] = { [0 ...(NUM_SIG - 1)] = SA_DFL };

static void __twix_do_handler(long *args)
{
	struct twix_queue_entry tqe;
	if(args[0] < 0 || args[0] >= NUM_SIG || args[0] == 0) {
		goto inform_done;
	}
	struct k_sigaction *action = &_signal_actions[args[0]];

	if(action->handler == SIG_IGN) {
		goto inform_done;
	} else if(action->handler == SIG_DFL) {
		switch(args[0]) {
			case SIGCHLD:
			case SIGURG:
			case SIGWINCH:
				break;
			case SIGCONT:
				break;
			case SIGTTIN:
			case SIGTTOU:
			case SIGSTOP:
			case SIGTSTP:
				tqe = build_tqe(TWIX_CMD_SUSPEND, 0, 0, 1, args[0]);
				twix_sync_command(&tqe);
				break;
			default: {
				struct twix_queue_entry tqe = build_tqe(
				  TWIX_CMD_EXIT, 0, 0, 2, args[0], TWIX_FLAGS_EXIT_THREAD | TWIX_FLAGS_EXIT_SIGNAL);
				twix_sync_command(&tqe);
				twz_thread_exit(args[0]);
			}
		}
	} else {
		action->handler(args[0]);
	}
inform_done:
	tqe = build_tqe(TWIX_CMD_SIGDONE, 0, 0, 1, args[0]);
	twix_sync_command(&tqe);
}

void check_signals(struct twix_conn *conn)
{
	for(size_t i = 0; i < conn->pending_count; i++) {
		long ex = 2;
		long args[3];
		args[0] = conn->pending_sigs[i].args[0];
		args[1] = conn->pending_sigs[i].args[1];
		args[2] = conn->pending_sigs[i].args[2];
		if(atomic_compare_exchange_strong(&conn->pending_sigs[i].flags, &ex, 0)) {
			__twix_do_handler(args);
		}
	}
}

static void append_signal(struct twix_conn *conn, long *args)
{
	for(size_t i = 0; i < conn->pending_count; i++) {
		long ex = 0;
		if(atomic_compare_exchange_strong(&conn->pending_sigs[i].flags, &ex, 1)) {
			conn->pending_sigs[i].args[0] = args[1];
			conn->pending_sigs[i].args[1] = args[2];
			conn->pending_sigs[i].args[2] = args[3];
			conn->pending_sigs[i].flags = 2;
			return;
		}
	}

	size_t new = conn->pending_count++;
	conn->pending_sigs[new].flags = 1;
	conn->pending_sigs[new].args[0] = args[1];
	conn->pending_sigs[new].args[1] = args[2];
	conn->pending_sigs[new].args[2] = args[3];
	conn->pending_sigs[new].flags = 2;
}

void __twix_signal_handler(int fault, void *data, void *userdata)
{
	(void)userdata;
	struct fault_signal_info *info = data;
	debug_printf("!!!!! SIGNAL HANDLER: %ld\n", info->args[1]);

	struct twix_conn *conn = get_twix_conn();
	append_signal(conn, info->args);

	if(conn->block_count == 0) {
		check_signals(conn);
	}
}

long hook_sigaction(struct syscall_args *args)
{
	int signum = args->a0;
	struct k_sigaction *act = (void *)args->a1;
	struct k_sigaction *oldact = (void *)args->a2;

	if(oldact) {
		*oldact = _signal_actions[signum];
	} else if(act) {
		/* TODO: need to make this atomic */
		_signal_actions[signum] = *act;
	}

	return 0;
}
