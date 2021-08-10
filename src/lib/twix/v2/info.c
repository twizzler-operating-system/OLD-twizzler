#include "v2.h"
#include <sys/resource.h>
#include <sys/utsname.h>
#include <twix/twix.h>
long hook_uname(struct syscall_args *args)
{
	struct utsname *buf = (void *)args->a0;
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_UNAME, 0, 0, 0);
	twix_sync_command(&tqe);
	if(tqe.ret == 0) {
		struct twix_uname_info info;
		extract_bufdata(&info, sizeof(info), 0);
		strcpy(buf->sysname, info.sysname);
		strcpy(buf->nodename, info.sysname);
		strcpy(buf->nodename, "twizzler"); // TODO
		strcpy(buf->release, info.release);
		strcpy(buf->version, info.version);
	}
	return tqe.ret;
}

long do_prlimit(int pid, int r, struct rlimit *new, struct rlimit *old)
{
	int flags = 0;
	if(new)
		flags |= TWIX_PRLIMIT_SET;
	if(old)
		flags |= TWIX_PRLIMIT_GET;
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_PRLIMIT, 0, sizeof(*new), 3, pid, r, flags);
	if(new)
		write_bufdata(new, sizeof(*new), 0);
	twix_sync_command(&tqe);
	if(old && tqe.ret == 0)
		extract_bufdata(old, sizeof(*old), 0);
	return tqe.ret;
}

long hook_prlimit(struct syscall_args *args)
{
	int pid = args->a0;
	int r = args->a1;
	struct rlimit *new = (struct rlimit *)args->a2;
	struct rlimit *old = (struct rlimit *)args->a3;
	return do_prlimit(pid, r, new, old);
}

long hook_getrlimit(struct syscall_args *args)
{
	int r = args->a0;
	struct rlimit *lim = (struct rlimit *)args->a1;
	return do_prlimit(0, r, NULL, lim);
}

long hook_setrlimit(struct syscall_args *args)
{
	int r = args->a0;
	struct rlimit *lim = (struct rlimit *)args->a1;
	return do_prlimit(0, r, NULL, lim);
}
