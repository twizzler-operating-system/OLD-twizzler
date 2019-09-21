#include "syscalls.h"
#include <errno.h>
#include <twz/_slots.h>
#include <twz/view.h>

static struct object unix_obj;
static bool unix_obj_init = false;
static struct unix_repr *uh;

void __linux_init(void)
{
	__fd_sys_init();
	if(!unix_obj_init) {
		unix_obj_init = true;
		uint32_t fl;
		twz_view_get(NULL, TWZSLOT_UNIX, NULL, &fl);
		if(!(fl & VE_VALID)) {
			objid_t id;
			twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &id);
			twz_view_set(NULL, TWZSLOT_UNIX, id, VE_READ | VE_WRITE);
			unix_obj = TWZ_OBJECT_INIT(TWZSLOT_UNIX);
			uh = twz_obj_base(&unix_obj);
			uh->pid = 1;
			uh->tid = 1;
		}

		uh = twz_obj_base(&unix_obj);
	}
}

#include <sys/utsname.h>
long linux_sys_uname(struct utsname *u)
{
	strcpy(u->sysname, "Twizzler");
	strcpy(u->nodename, "twizzler"); // TODO
	strcpy(u->release, "0.1");
	strcpy(u->version, "0.1");
	strcpy(u->machine, "x86_64");
	return 0;
}

long linux_sys_getuid(void)
{
	return uh ? uh->uid : -ENOSYS;
}

long linux_sys_getgid(void)
{
	return uh ? uh->gid : -ENOSYS;
}

long linux_sys_geteuid(void)
{
	return uh ? uh->euid : -ENOSYS;
}

long linux_sys_getegid(void)
{
	return uh ? uh->egid : -ENOSYS;
}
