#include <time.h>
#include <twix/twix.h>
#include <twz/obj.h>
#include <twz/sys/obj.h>
#include <twz/sys/thread.h>
#include <twz/sys/view.h>

#include "../syscalls.h"
#include "v2.h"

#include <sys/select.h>

long __hook_ppoll(struct pollfd *fds,
  long nfds,
  const struct timespec *spec,
  const sigset_t *sigmask)
{
	long flags = 0;
	struct twix_poll_info info;
	if(spec) {
		info.timeout = *spec;
		flags |= TWIX_POLL_TIMEOUT;
	}
	if(sigmask) {
		info.sigmask = *sigmask;
		flags |= TWIX_POLL_SIGMASK;
	}
	info.nr_polls = nfds;
	struct twix_queue_entry tqe = build_tqe(
	  TWIX_CMD_POLL, 0, sizeof(struct twix_poll_info) + sizeof(struct pollfd) * nfds, 1, flags);
	write_bufdata(&info, sizeof(info), 0);
	write_bufdata(fds, sizeof(fds[0]) * nfds, sizeof(info));
	twix_sync_command(&tqe);
	extract_bufdata(fds, sizeof(fds[0]) * nfds, sizeof(info));
	return tqe.ret;
}

long hook_poll(struct syscall_args *args)
{
	struct timespec spec;
	spec.tv_sec = args->a2 / 1000;
	spec.tv_nsec = args->a2 * 1000000;
	return __hook_ppoll((void *)args->a0, args->a1, args->a2 < 0 ? NULL : &spec, NULL);
}

long hook_ppoll(struct syscall_args *args)
{
	return __hook_ppoll((void *)args->a0, args->a1, (void *)args->a2, (void *)args->a3);
}

long __hook_pselect(int nfds,
  fd_set *restrict readfds,
  fd_set *restrict writefds,
  fd_set *restrict exceptfds,
  const struct timespec *restrict timeout,
  const sigset_t *restrict sigmask)
{
	struct pollfd polls[nfds];
	int poll_count = 0;
	for(int i = 0; i < nfds; i++) {
		int any = (readfds && FD_ISSET(i, readfds)) || (writefds && FD_ISSET(i, writefds))
		          || (exceptfds && FD_ISSET(i, exceptfds));
		if(any) {
			polls[poll_count].fd = i;
			polls[poll_count].revents = 0;
			polls[poll_count].events = 0;
			if(readfds && FD_ISSET(i, readfds)) {
				polls[poll_count].events |= POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR;
			}
			if(writefds && FD_ISSET(i, writefds)) {
				polls[poll_count].events |= POLLWRNORM | POLLWRBAND | POLLOUT | POLLERR;
			}
			if(exceptfds && FD_ISSET(i, exceptfds)) {
				polls[poll_count].events |= POLLPRI;
			}
			poll_count++;
		}
	}

	long ret = __hook_ppoll(polls, poll_count, timeout, sigmask);
	if(ret < 0) {
		return ret;
	}
	if(ret == 0) {
		if(readfds)
			FD_ZERO(readfds);
		if(writefds)
			FD_ZERO(writefds);
		if(exceptfds)
			FD_ZERO(exceptfds);
		return 0;
	}

	ret = 0;
	for(int i = 0; i < poll_count; i++) {
		int r = (readfds && FD_ISSET(polls[i].fd, readfds));
		int w = (writefds && FD_ISSET(polls[i].fd, writefds));
		int e = (exceptfds && FD_ISSET(polls[i].fd, exceptfds));

		int ready_r = polls[i].revents & (POLLRDNORM | POLLRDBAND | POLLHUP | POLLIN | POLLERR);
		int ready_w = polls[i].revents & (POLLWRNORM | POLLWRBAND | POLLOUT | POLLERR);
		int ready_e = polls[i].revents & (POLLPRI);

		if(r)
			FD_CLR(polls[i].fd, readfds);
		if(w)
			FD_CLR(polls[i].fd, writefds);
		if(e)
			FD_CLR(polls[i].fd, exceptfds);

		if(r && ready_r) {
			FD_SET(polls[i].fd, readfds);
			ret++;
		}
		if(w && ready_w) {
			FD_SET(polls[i].fd, writefds);
			ret++;
		}
		if(e && ready_e) {
			FD_SET(polls[i].fd, exceptfds);
			ret++;
		}
	}
	return ret;
}

long hook_pselect(struct syscall_args *args)
{
	struct sigi {
		sigset_t *s;
		size_t sl;
	};

	struct sigi *s = (void *)args->a5;

	return __hook_pselect(args->a0,
	  (void *)args->a1,
	  (void *)args->a2,
	  (void *)args->a3,
	  (void *)args->a4,
	  (void *)s ? s->s : NULL);
}

long hook_select(struct syscall_args *args)
{
	struct timespec spec;
	struct timeval *val = (void *)args->a4;
	if(val) {
		spec.tv_sec = val->tv_sec;
		spec.tv_nsec = val->tv_usec * 1000;
	}
	return __hook_pselect(
	  args->a0, (void *)args->a1, (void *)args->a2, (void *)args->a3, val ? &spec : NULL, NULL);
}
